#pragma once

#include <keet/error.hpp>
#include <keet/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <basetsd.h>
#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
using ssize_t = SSIZE_T;
#endif
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace keet::net {

namespace detail {

#if defined(_WIN32)
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;

inline void ensure_winsock_initialized() {
    struct WinsockInitializer {
        bool ok = false;
        int err = 0;
        WinsockInitializer() noexcept {
            WSADATA wsa_data{};
            err = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
            ok = (err == 0);
        }
        ~WinsockInitializer() noexcept {
            if (ok) {
                ::WSACleanup();
            }
        }
    };
    static WinsockInitializer init;
    if (!init.ok) {
        throw keet::error("NetError", "WSAStartup failed with error code " + std::to_string(init.err));
    }
}

inline void close_raw_socket(socket_t s) noexcept {
    if (s != kInvalidSocket) {
        ::closesocket(s);
    }
}

inline std::string last_socket_error_str() {
    int code = ::WSAGetLastError();
    return "Winsock error " + std::to_string(code);
}
#else
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;

inline void ensure_winsock_initialized() noexcept {}

inline void close_raw_socket(socket_t s) noexcept {
    if (s != kInvalidSocket) {
        ::close(s);
    }
}

inline std::string last_socket_error_str() {
    return std::string(std::strerror(errno));
}
#endif

inline void configure_no_sigpipe(socket_t s) noexcept {
#if defined(__APPLE__) && defined(SO_NOSIGPIPE)
    int one = 1;
    (void)::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, static_cast<socklen_t>(sizeof(one)));
#else
    (void)s;
#endif
}

inline int send_flags() noexcept {
#if defined(MSG_NOSIGNAL) && !defined(_WIN32)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

inline void set_nonblocking(socket_t s, bool nonblocking) {
#if defined(_WIN32)
    u_long mode = nonblocking ? 1UL : 0UL;
    if (::ioctlsocket(s, FIONBIO, &mode) != 0) {
        throw keet::error("NetError", "failed to set socket non-blocking mode: " + last_socket_error_str());
    }
#else
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        throw keet::error("NetError", "fcntl(F_GETFL) failed: " + last_socket_error_str());
    }
    int updated = nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(s, F_SETFL, updated) < 0) {
        throw keet::error("NetError", "fcntl(F_SETFL) failed: " + last_socket_error_str());
    }
#endif
}

inline void apply_socket_timeout_raw(socket_t s, int64_t timeout_ms, const char* fn_name) {
    if (timeout_ms < 0) {
        throw keet::error("NetError", std::string(fn_name) + ": timeoutMs must be >= 0");
    }
#if defined(_WIN32)
    DWORD tv = static_cast<DWORD>(timeout_ms);
    if (::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) != 0 ||
        ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv)) != 0) {
        throw keet::error("NetError", std::string(fn_name) + ": setsockopt timeout failed: " + last_socket_error_str());
    }
#else
    struct timeval tv{};
    tv.tv_sec = static_cast<time_t>(timeout_ms / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000) * 1000);
    if (::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, static_cast<socklen_t>(sizeof(tv))) != 0 ||
        ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, static_cast<socklen_t>(sizeof(tv))) != 0) {
        throw keet::error("NetError", std::string(fn_name) + ": setsockopt timeout failed: " + last_socket_error_str());
    }
#endif
}

// ============================================================================
// Thread-Safe Socket Handle Registry
// ============================================================================

enum class SocketKind {
    TcpStream,
    TcpListener,
    Udp
};

struct SocketEntry {
    socket_t fd = kInvalidSocket;
    SocketKind kind = SocketKind::TcpStream;
};

class SocketRegistry {
public:
    static SocketRegistry& instance() {
        static SocketRegistry reg;
        return reg;
    }

    int64_t register_socket(socket_t fd, SocketKind kind) {
        std::lock_guard<std::mutex> lock(mutex_);
        int64_t h = next_handle_++;
        entries_[h] = SocketEntry{fd, kind};
        return h;
    }

    socket_t get_socket(int64_t handle, SocketKind expected_kind, const char* fn_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(handle);
        if (it == entries_.end()) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": invalid or closed socket handle " + std::to_string(handle)
            );
        }
        if (it->second.kind != expected_kind) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": socket handle " + std::to_string(handle) + " has incompatible socket type"
            );
        }
        return it->second.fd;
    }

    socket_t get_tcp_any_socket(int64_t handle, const char* fn_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(handle);
        if (it == entries_.end()) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": invalid or closed socket handle " + std::to_string(handle)
            );
        }
        if (it->second.kind != SocketKind::TcpStream && it->second.kind != SocketKind::TcpListener) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": socket handle " + std::to_string(handle) + " is not a TCP socket"
            );
        }
        return it->second.fd;
    }

    void close_tcp_handle(int64_t handle, const char* fn_name) {
        socket_t fd = kInvalidSocket;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = entries_.find(handle);
            if (it == entries_.end()) {
                throw keet::error(
                    "NetError",
                    std::string(fn_name) + ": invalid or already closed TCP socket handle " + std::to_string(handle)
                );
            }
            if (it->second.kind != SocketKind::TcpStream && it->second.kind != SocketKind::TcpListener) {
                throw keet::error(
                    "NetError",
                    std::string(fn_name) + ": socket handle " + std::to_string(handle) + " is not a TCP socket"
                );
            }
            fd = it->second.fd;
            entries_.erase(it);
        }
        close_raw_socket(fd);
    }

    void close_udp_handle(int64_t handle, const char* fn_name) {
        socket_t fd = kInvalidSocket;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = entries_.find(handle);
            if (it == entries_.end()) {
                throw keet::error(
                    "NetError",
                    std::string(fn_name) + ": invalid or already closed UDP socket handle " + std::to_string(handle)
                );
            }
            if (it->second.kind != SocketKind::Udp) {
                throw keet::error(
                    "NetError",
                    std::string(fn_name) + ": socket handle " + std::to_string(handle) + " is not a UDP socket"
                );
            }
            fd = it->second.fd;
            entries_.erase(it);
        }
        close_raw_socket(fd);
    }

private:
    SocketRegistry() = default;
    std::mutex mutex_;
    int64_t next_handle_ = 1;
    std::unordered_map<int64_t, SocketEntry> entries_;
};

// ============================================================================
// URL Parsing & Helpers
// ============================================================================

struct ParsedUrl {
    std::string scheme;
    std::string host;
    int64_t port = -1;
    std::string port_str;
    std::string path;
    std::string query;
    std::string fragment;
};

[[nodiscard]] inline std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

[[nodiscard]] inline std::string trim_ascii(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

[[nodiscard]] inline int64_t default_port_for_scheme(std::string_view scheme) noexcept {
    if (scheme == "http" || scheme == "ws") return 80;
    if (scheme == "https" || scheme == "wss") return 443;
    if (scheme == "ftp") return 21;
    return -1;
}

[[nodiscard]] inline ParsedUrl parse_url_internal(const std::string& url, const char* fn_name) {
    if (url.empty()) {
        throw keet::error("NetError", std::string(fn_name) + ": URL cannot be empty");
    }

    size_t scheme_pos = url.find("://");
    if (scheme_pos == std::string::npos || scheme_pos == 0) {
        throw keet::error("NetError", std::string(fn_name) + ": missing or invalid scheme in URL '" + url + "'");
    }

    ParsedUrl result;
    result.scheme = to_lower_ascii(std::string_view(url).substr(0, scheme_pos));
    for (size_t i = 0; i < result.scheme.size(); ++i) {
        char c = result.scheme[i];
        bool valid = (i == 0) ? (std::isalpha(static_cast<unsigned char>(c)) != 0)
                              : (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '+' || c == '-' || c == '.');
        if (!valid) {
            throw keet::error("NetError", std::string(fn_name) + ": invalid scheme in URL '" + url + "'");
        }
    }

    size_t rest_start = scheme_pos + 3;
    size_t frag_pos = url.find('#', rest_start);
    std::string_view before_frag = (frag_pos == std::string::npos)
        ? std::string_view(url).substr(rest_start)
        : std::string_view(url).substr(rest_start, frag_pos - rest_start);
    if (frag_pos != std::string::npos) {
        result.fragment = url.substr(frag_pos + 1);
    }

    size_t query_pos = before_frag.find('?');
    std::string_view authority_and_path = (query_pos == std::string_view::npos)
        ? before_frag
        : before_frag.substr(0, query_pos);
    if (query_pos != std::string_view::npos) {
        result.query = std::string(before_frag.substr(query_pos + 1));
    }

    size_t path_pos = authority_and_path.find('/');
    std::string_view authority = (path_pos == std::string_view::npos)
        ? authority_and_path
        : authority_and_path.substr(0, path_pos);
    if (path_pos != std::string_view::npos) {
        result.path = std::string(authority_and_path.substr(path_pos));
    } else {
        result.path = "/";
    }

    // Strip optional userinfo@ if present
    size_t at_pos = authority.rfind('@');
    if (at_pos != std::string_view::npos) {
        authority = authority.substr(at_pos + 1);
    }

    if (authority.empty()) {
        throw keet::error("NetError", std::string(fn_name) + ": missing host in URL '" + url + "'");
    }

    std::string_view port_sv;
    if (authority.front() == '[') {
        // Bracketed IPv6 literal: [::1]:8080
        size_t close_bracket = authority.find(']');
        if (close_bracket == std::string_view::npos || close_bracket == 1) {
            throw keet::error("NetError", std::string(fn_name) + ": invalid IPv6 host in URL '" + url + "'");
        }
        result.host = std::string(authority.substr(1, close_bracket - 1));
        if (close_bracket + 1 < authority.size()) {
            if (authority[close_bracket + 1] != ':') {
                throw keet::error("NetError", std::string(fn_name) + ": invalid port separator in URL '" + url + "'");
            }
            port_sv = authority.substr(close_bracket + 2);
            if (port_sv.empty()) {
                throw keet::error("NetError", std::string(fn_name) + ": empty port after ':' in URL '" + url + "'");
            }
        }
    } else {
        size_t colon_pos = authority.rfind(':');
        if (colon_pos != std::string_view::npos) {
            result.host = std::string(authority.substr(0, colon_pos));
            port_sv = authority.substr(colon_pos + 1);
            if (port_sv.empty()) {
                throw keet::error("NetError", std::string(fn_name) + ": empty port after ':' in URL '" + url + "'");
            }
        } else {
            result.host = std::string(authority);
        }
    }

    if (result.host.empty()) {
        throw keet::error("NetError", std::string(fn_name) + ": empty host in URL '" + url + "'");
    }

    if (!port_sv.empty()) {
        int64_t p = 0;
        auto [ptr, ec] = std::from_chars(port_sv.data(), port_sv.data() + port_sv.size(), p);
        if (ec != std::errc{} || ptr != port_sv.data() + port_sv.size() || p < 1 || p > 65535) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": invalid port '" + std::string(port_sv) + "' in URL '" + url + "'"
            );
        }
        result.port = p;
        result.port_str = std::to_string(p);
    } else {
        result.port = default_port_for_scheme(result.scheme);
        if (result.port > 0) {
            result.port_str = std::to_string(result.port);
        }
    }

    return result;
}

[[nodiscard]] inline int hex_digit_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// ============================================================================
// Socket Connection, Send, Recv & Endpoint Formatting Helpers
// ============================================================================

inline socket_t connect_with_timeout_raw(
    const std::string& host,
    int64_t port,
    int64_t timeout_ms,
    const char* fn_name) {
    ensure_winsock_initialized();
    if (host.empty()) {
        throw keet::error("NetError", std::string(fn_name) + ": host cannot be empty");
    }
    if (port < 1 || port > 65535) {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": port must be in range 1..65535, got " + std::to_string(port)
        );
    }
    if (timeout_ms <= 0) {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": timeoutMs must be > 0, got " + std::to_string(timeout_ms)
        );
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int gai_rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (gai_rc != 0 || res == nullptr) {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": failed to resolve '" + host + ":" + port_str + "'"
        );
    }

    socket_t connected_sock = kInvalidSocket;
    std::string last_err = "connection refused";

    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        socket_t s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == kInvalidSocket) {
            last_err = last_socket_error_str();
            continue;
        }
        configure_no_sigpipe(s);

        try {
            set_nonblocking(s, true);
        } catch (...) {
            close_raw_socket(s);
            continue;
        }

        int rc = ::connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen));
        if (rc == 0) {
            set_nonblocking(s, false);
            apply_socket_timeout_raw(s, timeout_ms, fn_name);
            connected_sock = s;
            break;
        }

#if defined(_WIN32)
        int err = ::WSAGetLastError();
        bool in_progress = (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS);
#else
        int err = errno;
        bool in_progress = (err == EINPROGRESS || err == EWOULDBLOCK);
#endif
        if (!in_progress) {
            last_err = last_socket_error_str();
            close_raw_socket(s);
            continue;
        }

#if defined(_WIN32)
        fd_set write_set;
        fd_set err_set;
        FD_ZERO(&write_set);
        FD_ZERO(&err_set);
        FD_SET(s, &write_set);
        FD_SET(s, &err_set);
        struct timeval tv{};
        tv.tv_sec = static_cast<long>(timeout_ms / 1000);
        tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
        int sel = ::select(0, nullptr, &write_set, &err_set, &tv);
        if (sel <= 0 || FD_ISSET(s, &err_set) || !FD_ISSET(s, &write_set)) {
            last_err = (sel == 0) ? "connection timed out" : last_socket_error_str();
            close_raw_socket(s);
            continue;
        }
#else
        struct pollfd pfd{};
        pfd.fd = s;
        pfd.events = POLLOUT;
        int poll_rc = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
        if (poll_rc <= 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            last_err = (poll_rc == 0) ? "connection timed out" : last_socket_error_str();
            close_raw_socket(s);
            continue;
        }
#endif

        int so_error = 0;
        socklen_t len = static_cast<socklen_t>(sizeof(so_error));
#if defined(_WIN32)
        int gso = ::getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len);
#else
        int gso = ::getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &len);
#endif
        if (gso != 0 || so_error != 0) {
            last_err = "connect failed with socket error " + std::to_string(so_error);
            close_raw_socket(s);
            continue;
        }

        set_nonblocking(s, false);
        apply_socket_timeout_raw(s, timeout_ms, fn_name);
        connected_sock = s;
        break;
    }

    ::freeaddrinfo(res);

    if (connected_sock == kInvalidSocket) {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": failed to connect to '" + host + ":" + port_str + "' (" + last_err + ")"
        );
    }
    return connected_sock;
}

inline int64_t send_all_raw(socket_t s, std::string_view data, const char* fn_name) {
    size_t total_sent = 0;
    const char* ptr = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
#if defined(_WIN32)
        int chunk = static_cast<int>(std::min<size_t>(remaining, 65536));
        int n = ::send(s, ptr + total_sent, chunk, send_flags());
#else
        ssize_t n = ::send(s, ptr + total_sent, remaining, send_flags());
#endif
        if (n <= 0) {
            throw keet::error(
                "NetError",
                std::string(fn_name) + ": socket send failed: " + last_socket_error_str()
            );
        }
        total_sent += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
    return static_cast<int64_t>(total_sent);
}

[[nodiscard]] inline std::pair<std::string, int64_t> format_sockaddr(const struct sockaddr_storage& addr) {
    char ip_buf[INET6_ADDRSTRLEN] = {0};
    int64_t port = 0;
    if (addr.ss_family == AF_INET) {
        const auto* sin = reinterpret_cast<const struct sockaddr_in*>(&addr);
        ::inet_ntop(AF_INET, &sin->sin_addr, ip_buf, sizeof(ip_buf));
        port = ntohs(sin->sin_port);
    } else if (addr.ss_family == AF_INET6) {
        const auto* sin6 = reinterpret_cast<const struct sockaddr_in6*>(&addr);
        ::inet_ntop(AF_INET6, &sin6->sin6_addr, ip_buf, sizeof(ip_buf));
        port = ntohs(sin6->sin6_port);
    }
    return {std::string(ip_buf), port};
}

[[nodiscard]] inline int64_t query_local_port(socket_t s, const char* fn_name) {
    struct sockaddr_storage addr{};
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    if (::getsockname(s, reinterpret_cast<struct sockaddr*>(&addr), &len) != 0) {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": getsockname failed: " + last_socket_error_str()
        );
    }
    return format_sockaddr(addr).second;
}

// ============================================================================
// HTTP/1.1 & HTTPS Client Implementation
// ============================================================================

struct HttpResponseData {
    int64_t status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string final_url;
};

[[nodiscard]] inline std::string serialize_http_response_json(const HttpResponseData& resp) {
    std::string doc = keet::json::emptyObject();
    doc = keet::json::setInt(doc, "status", resp.status);
    doc = keet::json::setBool(doc, "ok", resp.status >= 200 && resp.status < 300);
    doc = keet::json::setString(doc, "body", resp.body);
    doc = keet::json::setString(doc, "url", resp.final_url);
    doc = keet::json::setRaw(doc, "headers", keet::json::stringify(resp.headers));
    return doc;
}

[[nodiscard]] inline std::string decode_chunked_body(std::string_view raw_chunked) {
    std::string out;
    size_t pos = 0;
    while (pos < raw_chunked.size()) {
        size_t line_end = raw_chunked.find("\r\n", pos);
        if (line_end == std::string_view::npos) {
            break;
        }
        std::string_view size_line = raw_chunked.substr(pos, line_end - pos);
        size_t semi = size_line.find(';');
        if (semi != std::string_view::npos) {
            size_line = size_line.substr(0, semi);
        }
        std::string trimmed = trim_ascii(size_line);
        if (trimmed.empty()) {
            break;
        }
        size_t chunk_len = 0;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), chunk_len, 16);
        if (ec != std::errc{}) {
            break;
        }
        pos = line_end + 2;
        if (chunk_len == 0) {
            break;
        }
        if (pos + chunk_len > raw_chunked.size()) {
            out.append(raw_chunked.substr(pos));
            break;
        }
        out.append(raw_chunked.substr(pos, chunk_len));
        pos += chunk_len;
        if (pos + 2 <= raw_chunked.size() && raw_chunked.substr(pos, 2) == "\r\n") {
            pos += 2;
        }
    }
    return out;
}

[[nodiscard]] inline HttpResponseData parse_raw_http_response(
    std::string_view raw,
    const std::string& final_url,
    const char* fn_name) {
    size_t hdr_end = raw.find("\r\n\r\n");
    size_t sep_len = 4;
    if (hdr_end == std::string_view::npos) {
        hdr_end = raw.find("\n\n");
        sep_len = 2;
    }
    if (hdr_end == std::string_view::npos) {
        throw keet::error("NetError", std::string(fn_name) + ": malformed HTTP response (missing header separator)");
    }

    std::string_view header_block = raw.substr(0, hdr_end);
    std::string_view body_block = raw.substr(hdr_end + sep_len);

    size_t first_line_end = header_block.find('\n');
    std::string_view status_line = (first_line_end == std::string_view::npos)
        ? header_block
        : header_block.substr(0, first_line_end);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.remove_suffix(1);
    }

    // Parse "HTTP/1.1 200 OK"
    size_t sp1 = status_line.find(' ');
    if (sp1 == std::string_view::npos) {
        throw keet::error("NetError", std::string(fn_name) + ": malformed HTTP status line");
    }
    std::string_view after_proto = status_line.substr(sp1 + 1);
    while (!after_proto.empty() && after_proto.front() == ' ') {
        after_proto.remove_prefix(1);
    }
    size_t sp2 = after_proto.find(' ');
    std::string_view code_sv = (sp2 == std::string_view::npos) ? after_proto : after_proto.substr(0, sp2);
    int64_t status_code = 0;
    auto [ptr, ec] = std::from_chars(code_sv.data(), code_sv.data() + code_sv.size(), status_code);
    if (ec != std::errc{} || status_code < 100 || status_code > 599) {
        throw keet::error("NetError", std::string(fn_name) + ": invalid HTTP status code in response");
    }

    HttpResponseData resp;
    resp.status = status_code;
    resp.final_url = final_url;

    if (first_line_end != std::string_view::npos) {
        size_t pos = first_line_end + 1;
        while (pos < header_block.size()) {
            size_t next_nl = header_block.find('\n', pos);
            std::string_view line = (next_nl == std::string_view::npos)
                ? header_block.substr(pos)
                : header_block.substr(pos, next_nl - pos);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string k = to_lower_ascii(trim_ascii(line.substr(0, colon)));
                std::string v = trim_ascii(line.substr(colon + 1));
                if (!k.empty()) {
                    resp.headers[k] = v;
                }
            }
            if (next_nl == std::string_view::npos) break;
            pos = next_nl + 1;
        }
    }

    auto te_it = resp.headers.find("transfer-encoding");
    if (te_it != resp.headers.end() && to_lower_ascii(te_it->second).find("chunked") != std::string::npos) {
        resp.body = decode_chunked_body(body_block);
    } else {
        auto cl_it = resp.headers.find("content-length");
        if (cl_it != resp.headers.end()) {
            size_t content_len = 0;
            auto [cptr, cec] = std::from_chars(
                cl_it->second.data(),
                cl_it->second.data() + cl_it->second.size(),
                content_len
            );
            if (cec == std::errc{} && content_len <= body_block.size()) {
                resp.body = std::string(body_block.substr(0, content_len));
            } else {
                resp.body = std::string(body_block);
            }
        } else {
            resp.body = std::string(body_block);
        }
    }

    return resp;
}

[[nodiscard]] inline HttpResponseData https_request_via_curl(
    const std::string& method,
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body,
    int64_t timeout_ms,
    const char* fn_name) {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = ++counter;
    auto tmp_dir = std::filesystem::temp_directory_path();
    auto hdr_file = tmp_dir / ("keety_net_hdr_" + std::to_string(id) + ".txt");
    auto body_file = tmp_dir / ("keety_net_body_" + std::to_string(id) + ".txt");
    auto req_file = tmp_dir / ("keety_net_req_" + std::to_string(id) + ".txt");

    auto cleanup = [&]() noexcept {
        std::error_code ec;
        std::filesystem::remove(hdr_file, ec);
        std::filesystem::remove(body_file, ec);
        std::filesystem::remove(req_file, ec);
    };

    auto quote_sh = [](const std::string& s) -> std::string {
#if defined(_WIN32)
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else out.push_back(c);
        }
        out.push_back('"');
        return out;
#else
        std::string out = "'";
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out.push_back(c);
        }
        out.push_back('\'');
        return out;
#endif
    };

    int64_t max_time_sec = std::max<int64_t>(1, (timeout_ms + 999) / 1000);
    std::ostringstream cmd;
    cmd << "curl -sS -L --max-redirs 5"
        << " -X " << quote_sh(method)
        << " --max-time " << max_time_sec
        << " -D " << quote_sh(hdr_file.string())
        << " -o " << quote_sh(body_file.string());

    for (const auto& [k, v] : headers) {
        cmd << " -H " << quote_sh(k + ": " + v);
    }

    if (!body.empty() || method == "POST" || method == "PUT" || method == "PATCH") {
        std::ofstream ofs(req_file, std::ios::binary);
        ofs.write(body.data(), static_cast<std::streamsize>(body.size()));
        ofs.close();
        cmd << " --data-binary " << quote_sh("@" + req_file.string());
    }

    cmd << " " << quote_sh(url);
#if defined(_WIN32)
    cmd << " >NUL 2>&1";
#else
    cmd << " >/dev/null 2>&1";
#endif

    int rc = std::system(cmd.str().c_str());
    if (rc != 0) {
        cleanup();
        throw keet::error("NetError", std::string(fn_name) + ": HTTPS request failed for URL '" + url + "'");
    }

    std::ifstream h_ifs(hdr_file, std::ios::binary);
    std::string raw_headers((std::istreambuf_iterator<char>(h_ifs)), std::istreambuf_iterator<char>());
    h_ifs.close();

    std::ifstream b_ifs(body_file, std::ios::binary);
    std::string resp_body((std::istreambuf_iterator<char>(b_ifs)), std::istreambuf_iterator<char>());
    b_ifs.close();
    cleanup();

    // If redirects occurred, curl writes multiple HTTP header blocks; parse the final HTTP status block
    size_t last_http = raw_headers.rfind("HTTP/");
    std::string_view final_hdr_block = (last_http == std::string::npos)
        ? std::string_view(raw_headers)
        : std::string_view(raw_headers).substr(last_http);

    std::string combined = std::string(final_hdr_block);
    auto has_suffix = [](std::string_view str, std::string_view sfx) noexcept -> bool {
        return str.size() >= sfx.size() && str.substr(str.size() - sfx.size()) == sfx;
    };
    if (!has_suffix(combined, "\r\n\r\n") && !has_suffix(combined, "\n\n")) {
        combined += "\r\n\r\n";
    }
    HttpResponseData parsed = parse_raw_http_response(combined, url, fn_name);
    parsed.body = std::move(resp_body);
    return parsed;
}

[[nodiscard]] inline HttpResponseData execute_http_request(
    const std::string& method,
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body,
    int64_t timeout_ms,
    const char* fn_name,
    int redirect_depth = 0) {
    if (redirect_depth > 5) {
        throw keet::error("NetError", std::string(fn_name) + ": maximum HTTP redirect limit (5) exceeded");
    }
    if (method.empty()) {
        throw keet::error("NetError", std::string(fn_name) + ": HTTP method cannot be empty");
    }
    if (timeout_ms <= 0) {
        throw keet::error("NetError", std::string(fn_name) + ": timeoutMs must be > 0");
    }

    std::string upper_method;
    upper_method.reserve(method.size());
    for (char c : method) {
        upper_method.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    ParsedUrl parsed = parse_url_internal(url, fn_name);
    if (parsed.scheme == "https") {
        return https_request_via_curl(upper_method, url, headers, body, timeout_ms, fn_name);
    }
    if (parsed.scheme != "http") {
        throw keet::error(
            "NetError",
            std::string(fn_name) + ": unsupported URL scheme '" + parsed.scheme + "' (expected 'http' or 'https')"
        );
    }

    socket_t sock = connect_with_timeout_raw(parsed.host, parsed.port, timeout_ms, fn_name);

    std::string target_path = parsed.path.empty() ? "/" : parsed.path;
    if (!parsed.query.empty()) {
        target_path += "?" + parsed.query;
    }

    bool has_host = false;
    bool has_user_agent = false;
    bool has_connection = false;
    bool has_content_length = false;

    for (const auto& [k, _] : headers) {
        std::string lk = to_lower_ascii(k);
        if (lk == "host") has_host = true;
        else if (lk == "user-agent") has_user_agent = true;
        else if (lk == "connection") has_connection = true;
        else if (lk == "content-length") has_content_length = true;
    }

    std::ostringstream req;
    req << upper_method << " " << target_path << " HTTP/1.1\r\n";
    if (!has_host) {
        if (parsed.port != 80 && parsed.port > 0) {
            req << "Host: " << parsed.host << ":" << parsed.port << "\r\n";
        } else {
            req << "Host: " << parsed.host << "\r\n";
        }
    }
    if (!has_user_agent) {
        req << "User-Agent: keety-net/0.2\r\n";
    }
    if (!has_connection) {
        req << "Connection: close\r\n";
    }
    for (const auto& [k, v] : headers) {
        req << k << ": " << v << "\r\n";
    }
    if (!has_content_length && (!body.empty() || upper_method == "POST" || upper_method == "PUT" || upper_method == "PATCH")) {
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "\r\n";
    req << body;

    std::string raw_req = req.str();
    try {
        (void)send_all_raw(sock, raw_req, fn_name);
    } catch (...) {
        close_raw_socket(sock);
        throw;
    }

    std::string raw_resp;
    std::array<char, 8192> buf{};
    for (;;) {
#if defined(_WIN32)
        int n = ::recv(sock, buf.data(), static_cast<int>(buf.size()), 0);
#else
        ssize_t n = ::recv(sock, buf.data(), buf.size(), 0);
#endif
        if (n > 0) {
            raw_resp.append(buf.data(), static_cast<size_t>(n));
        } else if (n == 0) {
            break;
        } else {
            std::string err = last_socket_error_str();
            close_raw_socket(sock);
            throw keet::error("NetError", std::string(fn_name) + ": failed reading HTTP response: " + err);
        }
    }
    close_raw_socket(sock);

    HttpResponseData resp = parse_raw_http_response(raw_resp, url, fn_name);

    // Handle 301, 302, 303, 307, 308 redirects if Location header is present
    if ((resp.status == 301 || resp.status == 302 || resp.status == 303 ||
         resp.status == 307 || resp.status == 308)) {
        auto loc_it = resp.headers.find("location");
        if (loc_it != resp.headers.end() && !loc_it->second.empty()) {
            std::string next_url = loc_it->second;
            if (next_url.front() == '/') {
                next_url = parsed.scheme + "://" + parsed.host +
                           (parsed.port != 80 ? (":" + std::to_string(parsed.port)) : "") + next_url;
            }
            std::string next_method = (resp.status == 303) ? "GET" : upper_method;
            std::string next_body = (resp.status == 303) ? "" : body;
            return execute_http_request(next_method, next_url, headers, next_body, timeout_ms, fn_name, redirect_depth + 1);
        }
    }

    return resp;
}

} // namespace detail

// ============================================================================
// 1. URL Parsing, Encoding & Query String Utilities
// ============================================================================

/// Percent-encodes unreserved characters per RFC 3986. Non-throwing.
[[nodiscard]] inline std::string urlEncode(const std::string& text) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 2);
    for (unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

/// Decodes a percent-encoded URL/query string (converting '+' to ' ').
/// Throws keet::error("NetError", ...) on malformed '%XX' escape sequences.
[[nodiscard]] inline std::string urlDecode(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%') {
            if (i + 2 >= text.size()) {
                throw keet::error("NetError", "urlDecode: incomplete percent-encoding at end of string");
            }
            int hi = detail::hex_digit_value(text[i + 1]);
            int lo = detail::hex_digit_value(text[i + 2]);
            if (hi < 0 || lo < 0) {
                throw keet::error(
                    "NetError",
                    std::string("urlDecode: invalid hex digits '%") + text[i + 1] + text[i + 2] + "'"
                );
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

/// Parses an absolute URL into a map with keys: "scheme", "host", "port", "path", "query", "fragment".
/// Throws keet::error("NetError", ...) if the URL is invalid.
[[nodiscard]] inline std::unordered_map<std::string, std::string> parseUrl(const std::string& url) {
    detail::ParsedUrl p = detail::parse_url_internal(url, "parseUrl");
    return {
        {"scheme", p.scheme},
        {"host", p.host},
        {"port", p.port_str},
        {"path", p.path},
        {"query", p.query},
        {"fragment", p.fragment}
    };
}

/// Extracts the lowercase scheme from url. Throws keet::error("NetError", ...) on invalid URL.
[[nodiscard]] inline std::string urlScheme(const std::string& url) {
    return detail::parse_url_internal(url, "urlScheme").scheme;
}

/// Extracts the host from url. Throws keet::error("NetError", ...) on invalid URL.
[[nodiscard]] inline std::string urlHost(const std::string& url) {
    return detail::parse_url_internal(url, "urlHost").host;
}

/// Extracts the port from url (or default port 80/443 for http/https, or -1).
/// Throws keet::error("NetError", ...) on invalid URL.
[[nodiscard]] inline int64_t urlPort(const std::string& url) {
    return detail::parse_url_internal(url, "urlPort").port;
}

/// Extracts the path from url (defaults to "/"). Throws keet::error("NetError", ...) on invalid URL.
[[nodiscard]] inline std::string urlPath(const std::string& url) {
    return detail::parse_url_internal(url, "urlPath").path;
}

/// Extracts the raw query string without leading '?'. Throws keet::error("NetError", ...) on invalid URL.
[[nodiscard]] inline std::string urlQuery(const std::string& url) {
    return detail::parse_url_internal(url, "urlQuery").query;
}

/// Builds a deterministic, lexicographically sorted URL query string ("k1=v1&k2=v2") from params.
[[nodiscard]] inline std::string buildQuery(const std::unordered_map<std::string, std::string>& params) {
    std::vector<std::string> keys;
    keys.reserve(params.size());
    for (const auto& [k, _] : params) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());

    std::string out;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) out.push_back('&');
        const std::string& k = keys[i];
        out += urlEncode(k);
        out.push_back('=');
        out += urlEncode(params.at(k));
    }
    return out;
}

/// Parses a URL query string (with or without leading '?') into a decoded map<string, string>.
/// Throws keet::error("NetError", ...) on invalid percent-encoding.
[[nodiscard]] inline std::unordered_map<std::string, std::string> parseQuery(const std::string& query) {
    std::unordered_map<std::string, std::string> result;
    std::string_view q(query);
    if (!q.empty() && q.front() == '?') {
        q.remove_prefix(1);
    }
    if (q.empty()) {
        return result;
    }

    size_t pos = 0;
    while (pos <= q.size()) {
        size_t amp = q.find('&', pos);
        std::string_view pair = (amp == std::string_view::npos)
            ? q.substr(pos)
            : q.substr(pos, amp - pos);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            if (eq == std::string_view::npos) {
                result[urlDecode(std::string(pair))] = "";
            } else {
                std::string k = urlDecode(std::string(pair.substr(0, eq)));
                std::string v = urlDecode(std::string(pair.substr(eq + 1)));
                result[std::move(k)] = std::move(v);
            }
        }
        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return result;
}

// ============================================================================
// 2. DNS Resolution & IP Address Utilities
// ============================================================================

/// Resolves host into a deduplicated array of numeric IPv4/IPv6 addresses.
/// Throws keet::error("NetError", ...) on failure.
[[nodiscard]] inline std::vector<std::string> resolveHost(const std::string& host) {
    detail::ensure_winsock_initialized();
    if (host.empty()) {
        throw keet::error("NetError", "resolveHost: host cannot be empty");
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || res == nullptr) {
        throw keet::error("NetError", "resolveHost: failed to resolve host '" + host + "'");
    }

    std::vector<std::string> addrs;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        char buf[INET6_ADDRSTRLEN] = {0};
        if (p->ai_family == AF_INET) {
            const auto* sin = reinterpret_cast<const struct sockaddr_in*>(p->ai_addr);
            if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)) != nullptr) {
                std::string ip(buf);
                if (std::find(addrs.begin(), addrs.end(), ip) == addrs.end()) {
                    addrs.push_back(std::move(ip));
                }
            }
        } else if (p->ai_family == AF_INET6) {
            const auto* sin6 = reinterpret_cast<const struct sockaddr_in6*>(p->ai_addr);
            if (::inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf)) != nullptr) {
                std::string ip(buf);
                if (std::find(addrs.begin(), addrs.end(), ip) == addrs.end()) {
                    addrs.push_back(std::move(ip));
                }
            }
        }
    }
    ::freeaddrinfo(res);

    if (addrs.empty()) {
        throw keet::error("NetError", "resolveHost: no IP addresses found for '" + host + "'");
    }
    return addrs;
}

/// Resolves host and returns the first IPv4 address. Throws keet::error("NetError", ...) if none found.
[[nodiscard]] inline std::string resolveIpv4(const std::string& host) {
    detail::ensure_winsock_initialized();
    if (host.empty()) {
        throw keet::error("NetError", "resolveIpv4: host cannot be empty");
    }

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (rc != 0 || res == nullptr) {
        throw keet::error("NetError", "resolveIpv4: failed to resolve IPv4 address for '" + host + "'");
    }

    std::string ipv4;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            char buf[INET_ADDRSTRLEN] = {0};
            const auto* sin = reinterpret_cast<const struct sockaddr_in*>(p->ai_addr);
            if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)) != nullptr) {
                ipv4 = buf;
                break;
            }
        }
    }
    ::freeaddrinfo(res);

    if (ipv4.empty()) {
        throw keet::error("NetError", "resolveIpv4: no IPv4 address found for '" + host + "'");
    }
    return ipv4;
}

/// Returns true if ip is a valid dotted-quad IPv4 address. Non-throwing.
[[nodiscard]] inline bool isIpv4(const std::string& ip) noexcept {
    if (ip.empty()) return false;
    struct in_addr addr{};
    return ::inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}

/// Returns true if ip is a valid IPv6 address. Non-throwing.
[[nodiscard]] inline bool isIpv6(const std::string& ip) noexcept {
    if (ip.empty()) return false;
    std::string candidate = ip;
    if (candidate.size() > 2 && candidate.front() == '[' && candidate.back() == ']') {
        candidate = candidate.substr(1, candidate.size() - 2);
    }
    struct in6_addr addr6{};
    return ::inet_pton(AF_INET6, candidate.c_str(), &addr6) == 1;
}

/// Returns true if ip is a valid IPv4 or IPv6 address. Non-throwing.
[[nodiscard]] inline bool isIp(const std::string& ip) noexcept {
    return isIpv4(ip) || isIpv6(ip);
}

/// Returns true if ip is "localhost", an IPv4 127.0.0.0/8 loopback address, or IPv6 ::1. Non-throwing.
[[nodiscard]] inline bool isLoopback(const std::string& ip) noexcept {
    std::string lower = detail::to_lower_ascii(ip);
    if (lower == "localhost" || lower == "::1" || lower == "[::1]" || lower == "0:0:0:0:0:0:0:1") {
        return true;
    }
    struct in_addr addr4{};
    if (::inet_pton(AF_INET, ip.c_str(), &addr4) == 1) {
        uint32_t host_order = ntohl(addr4.s_addr);
        return (host_order >> 24) == 127;
    }
    struct in6_addr addr6{};
    std::string candidate = ip;
    if (candidate.size() > 2 && candidate.front() == '[' && candidate.back() == ']') {
        candidate = candidate.substr(1, candidate.size() - 2);
    }
    if (::inet_pton(AF_INET6, candidate.c_str(), &addr6) == 1) {
        static const unsigned char kLoopback6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
        return std::memcmp(&addr6, kLoopback6, 16) == 0;
    }
    return false;
}

/// Returns the local system hostname. Throws keet::error("NetError", ...) on failure.
[[nodiscard]] inline std::string hostname() {
    detail::ensure_winsock_initialized();
    std::array<char, 256> buf{};
    if (::gethostname(buf.data(), static_cast<int>(buf.size() - 1)) != 0) {
        throw keet::error("NetError", "hostname: gethostname failed: " + detail::last_socket_error_str());
    }
    return std::string(buf.data());
}

// ============================================================================
// 3. HTTP/1.1 & HTTPS Client
// ============================================================================

/// Performs an HTTP/HTTPS GET request (30s timeout) and returns the response body.
[[nodiscard]] inline std::string get(const std::string& url) {
    return detail::execute_http_request("GET", url, {}, "", 30000, "get").body;
}

/// Performs an HTTP/HTTPS GET request (30s timeout) and returns the integer status code.
[[nodiscard]] inline int64_t getStatus(const std::string& url) {
    return detail::execute_http_request("GET", url, {}, "", 30000, "getStatus").status;
}

/// Performs an HTTP/HTTPS POST request with body and Content-Type header, returning the response body.
[[nodiscard]] inline std::string post(
    const std::string& url,
    const std::string& body,
    const std::string& contentType) {
    std::unordered_map<std::string, std::string> hdrs = {{"Content-Type", contentType}};
    return detail::execute_http_request("POST", url, hdrs, body, 30000, "post").body;
}

/// Performs an HTTP/HTTPS POST request with application/json content type and returns the response body.
[[nodiscard]] inline std::string postJson(const std::string& url, const std::string& jsonBody) {
    std::unordered_map<std::string, std::string> hdrs = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"}
    };
    return detail::execute_http_request("POST", url, hdrs, jsonBody, 30000, "postJson").body;
}

/// Performs an HTTP/HTTPS PUT request with body and Content-Type header, returning the response body.
[[nodiscard]] inline std::string put(
    const std::string& url,
    const std::string& body,
    const std::string& contentType) {
    std::unordered_map<std::string, std::string> hdrs = {{"Content-Type", contentType}};
    return detail::execute_http_request("PUT", url, hdrs, body, 30000, "put").body;
}

/// Performs an HTTP/HTTPS DELETE request and returns the response body.
[[nodiscard]] inline std::string deleteReq(const std::string& url) {
    return detail::execute_http_request("DELETE", url, {}, "", 30000, "deleteReq").body;
}

/// Executes a custom HTTP/HTTPS request and returns a JSON response object:
/// {"status":200,"ok":true,"body":"...","headers":{...},"url":"..."}
[[nodiscard]] inline std::string request(
    const std::string& method,
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body,
    int64_t timeoutMs) {
    detail::HttpResponseData resp = detail::execute_http_request(method, url, headers, body, timeoutMs, "request");
    return detail::serialize_http_response_json(resp);
}

/// Downloads url to destPath. Throws keet::error("NetError", ...) on HTTP status >= 400 or file write failure.
inline bool download(const std::string& url, const std::string& destPath) {
    if (destPath.empty()) {
        throw keet::error("NetError", "download: destPath cannot be empty");
    }
    detail::HttpResponseData resp = detail::execute_http_request("GET", url, {}, "", 60000, "download");
    if (resp.status >= 400) {
        throw keet::error(
            "NetError",
            "download: HTTP request returned error status " + std::to_string(resp.status) + " for '" + url + "'"
        );
    }
    std::ofstream ofs(destPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        throw keet::error("NetError", "download: cannot open destination file '" + destPath + "' for writing");
    }
    ofs.write(resp.body.data(), static_cast<std::streamsize>(resp.body.size()));
    if (!ofs.good()) {
        throw keet::error("NetError", "download: failed writing to file '" + destPath + "'");
    }
    ofs.flush();
    return true;
}

// ============================================================================
// 4. Handle-Based TCP Sockets
// ============================================================================

/// Connects a TCP client socket to host:port with a 10,000 ms default timeout and returns a socket handle.
[[nodiscard]] inline int64_t tcpConnect(const std::string& host, int64_t port) {
    detail::socket_t s = detail::connect_with_timeout_raw(host, port, 10000, "tcpConnect");
    return detail::SocketRegistry::instance().register_socket(s, detail::SocketKind::TcpStream);
}

/// Connects a TCP client socket to host:port with an explicit timeout in ms and returns a socket handle.
[[nodiscard]] inline int64_t tcpConnectTimeout(const std::string& host, int64_t port, int64_t timeoutMs) {
    detail::socket_t s = detail::connect_with_timeout_raw(host, port, timeoutMs, "tcpConnectTimeout");
    return detail::SocketRegistry::instance().register_socket(s, detail::SocketKind::TcpStream);
}

/// Binds and listens on host:port (port == 0 binds an OS ephemeral port) and returns a TCP listener handle.
[[nodiscard]] inline int64_t tcpListen(const std::string& host, int64_t port, int64_t backlog) {
    detail::ensure_winsock_initialized();
    if (host.empty()) {
        throw keet::error("NetError", "tcpListen: host cannot be empty");
    }
    if (port < 0 || port > 65535) {
        throw keet::error("NetError", "tcpListen: port must be in range 0..65535, got " + std::to_string(port));
    }
    if (backlog <= 0) {
        throw keet::error("NetError", "tcpListen: backlog must be > 0, got " + std::to_string(backlog));
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    const char* node = (host == "0.0.0.0" || host == "::") ? host.c_str() : host.c_str();
    int rc = ::getaddrinfo(node, port_str.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
        throw keet::error("NetError", "tcpListen: failed to resolve bind address '" + host + ":" + port_str + "'");
    }

    detail::socket_t listen_sock = detail::kInvalidSocket;
    std::string last_err = "bind failed";

    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        detail::socket_t s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == detail::kInvalidSocket) {
            last_err = detail::last_socket_error_str();
            continue;
        }

        int opt = 1;
#if defined(_WIN32)
        (void)::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
        (void)::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof(opt)));
#endif

        if (::bind(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) != 0) {
            last_err = detail::last_socket_error_str();
            detail::close_raw_socket(s);
            continue;
        }

        if (::listen(s, static_cast<int>(backlog)) != 0) {
            last_err = detail::last_socket_error_str();
            detail::close_raw_socket(s);
            continue;
        }

        listen_sock = s;
        break;
    }

    ::freeaddrinfo(res);

    if (listen_sock == detail::kInvalidSocket) {
        throw keet::error(
            "NetError",
            "tcpListen: failed to bind/listen on '" + host + ":" + port_str + "' (" + last_err + ")"
        );
    }

    return detail::SocketRegistry::instance().register_socket(listen_sock, detail::SocketKind::TcpListener);
}

/// Accepts an incoming connection on serverHandle and returns a new connected TCP stream handle.
[[nodiscard]] inline int64_t tcpAccept(int64_t serverHandle) {
    detail::socket_t srv = detail::SocketRegistry::instance().get_socket(
        serverHandle, detail::SocketKind::TcpListener, "tcpAccept"
    );
    struct sockaddr_storage client_addr{};
    socklen_t addr_len = static_cast<socklen_t>(sizeof(client_addr));
    detail::socket_t client_sock = ::accept(srv, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_sock == detail::kInvalidSocket) {
        throw keet::error("NetError", "tcpAccept: accept failed: " + detail::last_socket_error_str());
    }
    detail::configure_no_sigpipe(client_sock);
    return detail::SocketRegistry::instance().register_socket(client_sock, detail::SocketKind::TcpStream);
}

/// Returns the local bound port (1..65535) of a TCP stream or listener handle.
[[nodiscard]] inline int64_t tcpLocalPort(int64_t handle) {
    detail::socket_t s = detail::SocketRegistry::instance().get_tcp_any_socket(handle, "tcpLocalPort");
    return detail::query_local_port(s, "tcpLocalPort");
}

/// Returns the remote peer address ("ip:port") of a connected TCP stream handle.
[[nodiscard]] inline std::string tcpPeerAddr(int64_t handle) {
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::TcpStream, "tcpPeerAddr"
    );
    struct sockaddr_storage addr{};
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    if (::getpeername(s, reinterpret_cast<struct sockaddr*>(&addr), &len) != 0) {
        throw keet::error("NetError", "tcpPeerAddr: getpeername failed: " + detail::last_socket_error_str());
    }
    auto [ip, port] = detail::format_sockaddr(addr);
    return ip + ":" + std::to_string(port);
}

/// Sends all bytes of data over a connected TCP stream handle and returns the number of bytes sent.
inline int64_t tcpSend(int64_t handle, const std::string& data) {
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::TcpStream, "tcpSend"
    );
    return detail::send_all_raw(s, data, "tcpSend");
}

/// Receives up to maxBytes from a connected TCP stream handle (returns "" on clean EOF).
[[nodiscard]] inline std::string tcpRecv(int64_t handle, int64_t maxBytes) {
    if (maxBytes <= 0) {
        throw keet::error("NetError", "tcpRecv: maxBytes must be > 0, got " + std::to_string(maxBytes));
    }
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::TcpStream, "tcpRecv"
    );
    size_t cap = static_cast<size_t>(std::min<int64_t>(maxBytes, 16 * 1024 * 1024));
    std::string buf(cap, '\0');
#if defined(_WIN32)
    int n = ::recv(s, buf.data(), static_cast<int>(cap), 0);
#else
    ssize_t n = ::recv(s, buf.data(), cap, 0);
#endif
    if (n < 0) {
        throw keet::error("NetError", "tcpRecv: socket recv failed: " + detail::last_socket_error_str());
    }
    buf.resize(static_cast<size_t>(n));
    return buf;
}

/// Reads a single '\n'-delimited line (up to maxBytes) from a TCP stream handle, stripping trailing '\r\n' or '\n'.
[[nodiscard]] inline std::string tcpRecvLine(int64_t handle, int64_t maxBytes) {
    if (maxBytes <= 0) {
        throw keet::error("NetError", "tcpRecvLine: maxBytes must be > 0, got " + std::to_string(maxBytes));
    }
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::TcpStream, "tcpRecvLine"
    );
    size_t limit = static_cast<size_t>(std::min<int64_t>(maxBytes, 16 * 1024 * 1024));
    std::string line;
    char ch = '\0';
    while (line.size() < limit) {
#if defined(_WIN32)
        int n = ::recv(s, &ch, 1, 0);
#else
        ssize_t n = ::recv(s, &ch, 1, 0);
#endif
        if (n == 0) {
            break;
        }
        if (n < 0) {
            throw keet::error("NetError", "tcpRecvLine: socket recv failed: " + detail::last_socket_error_str());
        }
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

/// Sets receive and send timeouts (in milliseconds) on a TCP socket handle.
inline void tcpSetTimeout(int64_t handle, int64_t timeoutMs) {
    detail::socket_t s = detail::SocketRegistry::instance().get_tcp_any_socket(handle, "tcpSetTimeout");
    detail::apply_socket_timeout_raw(s, timeoutMs, "tcpSetTimeout");
}

/// Closes a TCP stream or listener handle and removes it from the handle table.
inline void tcpClose(int64_t handle) {
    detail::SocketRegistry::instance().close_tcp_handle(handle, "tcpClose");
}

// ============================================================================
// 5. Handle-Based UDP Sockets
// ============================================================================

/// Creates and binds a UDP socket on host:port (port == 0 binds an OS ephemeral port) and returns a handle.
[[nodiscard]] inline int64_t udpBind(const std::string& host, int64_t port) {
    detail::ensure_winsock_initialized();
    if (host.empty()) {
        throw keet::error("NetError", "udpBind: host cannot be empty");
    }
    if (port < 0 || port > 65535) {
        throw keet::error("NetError", "udpBind: port must be in range 0..65535, got " + std::to_string(port));
    }

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
        throw keet::error("NetError", "udpBind: failed to resolve bind address '" + host + ":" + port_str + "'");
    }

    detail::socket_t udp_sock = detail::kInvalidSocket;
    std::string last_err = "bind failed";

    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        detail::socket_t s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == detail::kInvalidSocket) {
            last_err = detail::last_socket_error_str();
            continue;
        }
        if (::bind(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) != 0) {
            last_err = detail::last_socket_error_str();
            detail::close_raw_socket(s);
            continue;
        }
        udp_sock = s;
        break;
    }

    ::freeaddrinfo(res);

    if (udp_sock == detail::kInvalidSocket) {
        throw keet::error("NetError", "udpBind: failed to bind UDP socket on '" + host + ":" + port_str + "' (" + last_err + ")");
    }

    return detail::SocketRegistry::instance().register_socket(udp_sock, detail::SocketKind::Udp);
}

/// Returns the local bound port (1..65535) of a UDP socket handle.
[[nodiscard]] inline int64_t udpLocalPort(int64_t handle) {
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::Udp, "udpLocalPort"
    );
    return detail::query_local_port(s, "udpLocalPort");
}

/// Sends a UDP datagram from handle to host:port and returns the number of bytes sent.
inline int64_t udpSendTo(int64_t handle, const std::string& host, int64_t port, const std::string& data) {
    if (host.empty()) {
        throw keet::error("NetError", "udpSendTo: host cannot be empty");
    }
    if (port < 1 || port > 65535) {
        throw keet::error("NetError", "udpSendTo: port must be in range 1..65535, got " + std::to_string(port));
    }
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::Udp, "udpSendTo"
    );

    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
        throw keet::error("NetError", "udpSendTo: failed to resolve destination '" + host + ":" + port_str + "'");
    }

#if defined(_WIN32)
    int n = ::sendto(
        s,
        data.data(),
        static_cast<int>(data.size()),
        0,
        res->ai_addr,
        static_cast<int>(res->ai_addrlen)
    );
#else
    ssize_t n = ::sendto(
        s,
        data.data(),
        data.size(),
        0,
        res->ai_addr,
        static_cast<socklen_t>(res->ai_addrlen)
    );
#endif
    ::freeaddrinfo(res);

    if (n < 0) {
        throw keet::error("NetError", "udpSendTo: sendto failed: " + detail::last_socket_error_str());
    }
    return static_cast<int64_t>(n);
}

/// Receives a UDP datagram up to maxBytes and returns a JSON string:
/// {"data":"...","host":"127.0.0.1","port":12345}
[[nodiscard]] inline std::string udpRecvFrom(int64_t handle, int64_t maxBytes) {
    if (maxBytes <= 0) {
        throw keet::error("NetError", "udpRecvFrom: maxBytes must be > 0, got " + std::to_string(maxBytes));
    }
    detail::socket_t s = detail::SocketRegistry::instance().get_socket(
        handle, detail::SocketKind::Udp, "udpRecvFrom"
    );
    size_t cap = static_cast<size_t>(std::min<int64_t>(maxBytes, 65536));
    std::string buf(cap, '\0');
    struct sockaddr_storage peer_addr{};
    socklen_t addr_len = static_cast<socklen_t>(sizeof(peer_addr));

#if defined(_WIN32)
    int n = ::recvfrom(
        s,
        buf.data(),
        static_cast<int>(cap),
        0,
        reinterpret_cast<struct sockaddr*>(&peer_addr),
        &addr_len
    );
#else
    ssize_t n = ::recvfrom(
        s,
        buf.data(),
        cap,
        0,
        reinterpret_cast<struct sockaddr*>(&peer_addr),
        &addr_len
    );
#endif
    if (n < 0) {
        throw keet::error("NetError", "udpRecvFrom: recvfrom failed: " + detail::last_socket_error_str());
    }
    buf.resize(static_cast<size_t>(n));
    auto [peer_ip, peer_port] = detail::format_sockaddr(peer_addr);

    std::string doc = keet::json::emptyObject();
    doc = keet::json::setString(doc, "data", buf);
    doc = keet::json::setString(doc, "host", peer_ip);
    doc = keet::json::setInt(doc, "port", peer_port);
    return doc;
}

/// Closes a UDP socket handle and removes it from the handle table.
inline void udpClose(int64_t handle) {
    detail::SocketRegistry::instance().close_udp_handle(handle, "udpClose");
}

} // namespace keet::net
