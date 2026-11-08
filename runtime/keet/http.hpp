#pragma once

#include <keet/crypto.hpp>
#include <keet/error.hpp>
#include <keet/json.hpp>
#include <keet/net.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>

#if defined(_WIN32)
#include <basetsd.h>
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
using ssize_t = SSIZE_T;
#endif
#endif
#include <mutex>
#include <optional>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace keet::net::http {

// ============================================================================
// Case-Insensitive Header Map
// ============================================================================

namespace detail {

inline std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

inline std::string to_upper_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

inline std::vector<std::string> split_path(std::string_view path) {
    std::vector<std::string> segs;
    size_t i = 0;
    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') {
            ++i;
        }
        if (i >= path.size()) {
            break;
        }
        size_t start = i;
        while (i < path.size() && path[i] != '/') {
            ++i;
        }
        segs.emplace_back(path.substr(start, i - start));
    }
    return segs;
}

inline std::string_view trim_ascii(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
        --end;
    }
    return s.substr(start, end - start);
}

inline bool safe_gmtime(const std::time_t* sec, std::tm* result) noexcept {
#if defined(_WIN32)
    return gmtime_s(result, sec) == 0;
#else
    return gmtime_r(sec, result) != nullptr;
#endif
}

} // namespace detail

class HeaderMap {
public:
    HeaderMap() = default;

    void set(std::string_view name, std::string_view value) {
        std::string key_lower = detail::to_lower_ascii(name);
        for (auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                entry.second = std::string(value);
                return;
            }
        }
        entries_.emplace_back(std::string(name), std::string(value));
    }

    void add(std::string_view name, std::string_view value) {
        std::string key_lower = detail::to_lower_ascii(name);
        for (auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                if (key_lower == "set-cookie") {
                    // Set-Cookie headers must be separate entries
                    entries_.emplace_back(std::string(name), std::string(value));
                } else {
                    entry.second += ", ";
                    entry.second += value;
                }
                return;
            }
        }
        entries_.emplace_back(std::string(name), std::string(value));
    }

    [[nodiscard]] bool has(std::string_view name) const {
        std::string key_lower = detail::to_lower_ascii(name);
        for (const auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::optional<std::string> get(std::string_view name) const {
        std::string key_lower = detail::to_lower_ascii(name);
        for (const auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                return entry.second;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string get_or(std::string_view name, std::string_view fallback = "") const {
        auto val = get(name);
        return val.has_value() ? *val : std::string(fallback);
    }

    bool remove(std::string_view name) {
        std::string key_lower = detail::to_lower_ascii(name);
        auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
            return detail::to_lower_ascii(entry.first) == key_lower;
        });
        if (it != entries_.end()) {
            entries_.erase(it, entries_.end());
            return true;
        }
        return false;
    }

    void clear() noexcept {
        entries_.clear();
    }

    [[nodiscard]] size_t size() const noexcept {
        return entries_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return entries_.empty();
    }

    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>>& entries() noexcept {
        return entries_;
    }

    [[nodiscard]] auto begin() const noexcept { return entries_.begin(); }
    [[nodiscard]] auto end() const noexcept { return entries_.end(); }
    [[nodiscard]] auto begin() noexcept { return entries_.begin(); }
    [[nodiscard]] auto end() noexcept { return entries_.end(); }

private:
    std::vector<std::pair<std::string, std::string>> entries_;
};

// ============================================================================
// URL Decoding & Query String Parser
// ============================================================================

[[nodiscard]] inline std::string url_decode(std::string_view in, bool plus_as_space = false) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '%' && i + 2 < in.size()) {
            char h1 = in[i + 1];
            char h2 = in[i + 2];
            auto hex_val = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            int v1 = hex_val(h1);
            int v2 = hex_val(h2);
            if (v1 >= 0 && v2 >= 0) {
                out.push_back(static_cast<char>((v1 << 4) | v2));
                i += 2;
                continue;
            }
        } else if (c == '+' && plus_as_space) {
            out.push_back(' ');
            continue;
        }
        out.push_back(c);
    }
    return out;
}

[[nodiscard]] inline std::unordered_map<std::string, std::string> parse_query_string(std::string_view query) {
    std::unordered_map<std::string, std::string> result;
    if (query.empty()) {
        return result;
    }
    if (query.front() == '?') {
        query.remove_prefix(1);
    }

    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find_first_of(";&", pos);
        std::string_view pair = (amp == std::string_view::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
        pos = (amp == std::string_view::npos) ? query.size() : amp + 1;

        if (pair.empty()) {
            continue;
        }

        size_t eq = pair.find('=');
        if (eq == std::string_view::npos) {
            std::string key = url_decode(pair, true);
            result[key] = "";
        } else {
            std::string key = url_decode(pair.substr(0, eq), true);
            std::string val = url_decode(pair.substr(eq + 1), true);
            result[key] = val;
        }
    }
    return result;
}

// ============================================================================
// HTTP Status Codes & RFC 9110 Reason Phrases
// ============================================================================

[[nodiscard]] inline std::string_view status_reason_phrase(int status) noexcept {
    switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 418: return "I'm a teapot";
    case 422: return "Unprocessable Entity";
    case 426: return "Upgrade Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    default:
        if (status >= 200 && status < 300) return "OK";
        if (status >= 300 && status < 400) return "Redirect";
        if (status >= 400 && status < 500) return "Client Error";
        if (status >= 500 && status < 600) return "Server Error";
        return "Unknown Status";
    }
}

[[nodiscard]] inline std::string format_http_date(
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) {
    std::time_t sec = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
    if (!detail::safe_gmtime(&sec, &tm_buf)) {
        return "Wed, 01 Jan 2026 00:00:00 GMT";
    }

    static const char* kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
                  kDays[tm_buf.tm_wday % 7],
                  tm_buf.tm_mday,
                  kMonths[tm_buf.tm_mon % 12],
                  tm_buf.tm_year + 1900,
                  tm_buf.tm_hour,
                  tm_buf.tm_min,
                  tm_buf.tm_sec);
    return std::string(buf);
}

// ============================================================================
// HTTP Request Message & Parser
// ============================================================================

struct RequestLine {
    std::string method;       ///< "GET", "POST", "PUT", etc.
    std::string raw_target;   ///< e.g. "/api/users?active=1"
    std::string path;         ///< e.g. "/api/users"
    std::string query_string; ///< e.g. "active=1"
    std::string version;      ///< "HTTP/1.1"
};

struct HttpRequestMessage {
    RequestLine line;
    HeaderMap headers;
    std::unordered_map<std::string, std::string> query;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> locals;
    std::unordered_map<std::string, std::string> body_params;
    std::string raw_body;
    std::shared_ptr<keet::json::JsonNode> json_value;
    std::string body;
    std::string client_ip = "127.0.0.1";
    int client_port = 0;
    bool keep_alive = true;
    bool upgrade_websocket = false;

    [[nodiscard]] std::string param(std::string_view name, std::string_view fallback = "") const {
        auto it = params.find(std::string(name));
        return (it != params.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string body_param(std::string_view name, std::string_view fallback = "") const {
        auto it = body_params.find(std::string(name));
        return (it != body_params.end()) ? it->second : std::string(fallback);
    }
};

enum class ParseState {
    RequestLine,
    Headers,
    BodyFixed,
    BodyChunkSize,
    BodyChunkData,
    BodyChunkCRLF,
    Trailers,
    Complete,
    Error
};

struct HttpParserLimits {
    size_t max_header_size = 64 * 1024;        ///< 64 KB header limit
    size_t max_body_size = 10 * 1024 * 1024;   ///< 10 MB body limit
};

class HttpRequestParser {
public:
    explicit HttpRequestParser(HttpParserLimits limits = {})
        : limits_(limits) {}

    void reset_keep_buffer() {
        state_ = ParseState::RequestLine;
        message_ = HttpRequestMessage{};
        error_code_ = 200;
        error_msg_.clear();
        content_length_ = 0;
        body_bytes_read_ = 0;
        current_chunk_size_ = 0;
        current_chunk_read_ = 0;
        is_chunked_ = false;
    }

    void reset() {
        buffer_.clear();
        reset_keep_buffer();
    }

    [[nodiscard]] ParseState state() const noexcept { return state_; }
    [[nodiscard]] bool is_complete() const noexcept { return state_ == ParseState::Complete; }
    [[nodiscard]] bool is_error() const noexcept { return state_ == ParseState::Error; }
    [[nodiscard]] int error_code() const noexcept { return error_code_; }
    [[nodiscard]] const std::string& error_message() const noexcept { return error_msg_; }
    [[nodiscard]] const HttpRequestMessage& message() const noexcept { return message_; }
    [[nodiscard]] HttpRequestMessage& message() noexcept { return message_; }
    [[nodiscard]] const std::string& buffer() const noexcept { return buffer_; }
    [[nodiscard]] std::string take_buffer() {
        std::string b = std::move(buffer_);
        buffer_.clear();
        return b;
    }

    /// Feeds incoming raw socket bytes to the incremental HTTP state machine.
    /// Returns true if more data is accepted or complete, false on unrecoverable protocol error.
    bool feed(std::string_view data) {
        if (state_ == ParseState::Complete || state_ == ParseState::Error) {
            return false;
        }

        buffer_.append(data);

        while (true) {
            switch (state_) {
            case ParseState::RequestLine: {
                size_t line_end = find_crlf(buffer_, 0);
                if (line_end == std::string::npos) {
                    if (buffer_.size() > limits_.max_header_size) {
                        return fail(431, "Request header line exceeds maximum allowed size");
                    }
                    return true; // Need more data
                }

                std::string line = buffer_.substr(0, line_end);
                size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                buffer_.erase(0, line_end + crlf_len);

                if (!parse_request_line(line)) {
                    return false;
                }
                state_ = ParseState::Headers;
                break;
            }

            case ParseState::Headers: {
                while (true) {
                    size_t line_end = find_crlf(buffer_, 0);
                    if (line_end == std::string::npos) {
                        if (buffer_.size() > limits_.max_header_size) {
                            return fail(431, "HTTP headers exceed maximum allowed size");
                        }
                        return true; // Need more data
                    }

                    if (line_end == 0 || (line_end == 1 && buffer_[0] == '\r')) {
                        // Empty line marks end of headers
                        size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                        buffer_.erase(0, line_end + crlf_len);
                        if (!transition_to_body()) {
                            return false;
                        }
                        break;
                    }

                    std::string header_line = buffer_.substr(0, line_end);
                    size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                    buffer_.erase(0, line_end + crlf_len);

                    if (!parse_header_line(header_line)) {
                        return false;
                    }
                }
                break;
            }

            case ParseState::BodyFixed: {
                if (content_length_ == 0) {
                    state_ = ParseState::Complete;
                    return true;
                }

                size_t needed = content_length_ - body_bytes_read_;
                size_t available = buffer_.size();
                size_t to_take = std::min(needed, available);

                message_.body.append(buffer_.substr(0, to_take));
                buffer_.erase(0, to_take);
                body_bytes_read_ += to_take;

                if (body_bytes_read_ >= content_length_) {
                    state_ = ParseState::Complete;
                    return true;
                }
                return true; // Need more body data
            }

            case ParseState::BodyChunkSize: {
                size_t line_end = find_crlf(buffer_, 0);
                if (line_end == std::string::npos) {
                    return true; // Need more data
                }

                std::string_view raw_line = std::string_view(buffer_).substr(0, line_end);
                std::string_view size_line = detail::trim_ascii(raw_line);

                // Strip chunk extensions (e.g. "1A;ext=val")
                size_t semi = size_line.find(';');
                if (semi != std::string_view::npos) {
                    size_line = detail::trim_ascii(size_line.substr(0, semi));
                }

                if (size_line.empty()) {
                    return fail(400, "Empty chunk size in chunked transfer encoding");
                }

                size_t chunk_len = 0;
                auto [ptr, ec] = std::from_chars(size_line.data(), size_line.data() + size_line.size(), chunk_len, 16);
                if (ec != std::errc{} || ptr != size_line.data() + size_line.size()) {
                    return fail(400, "Invalid hex chunk size: " + std::string(size_line));
                }

                size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                buffer_.erase(0, line_end + crlf_len);

                current_chunk_size_ = chunk_len;
                current_chunk_read_ = 0;

                if (current_chunk_size_ == 0) {
                    state_ = ParseState::Trailers;
                } else {
                    if (message_.body.size() + current_chunk_size_ > limits_.max_body_size) {
                        return fail(413, "Chunked payload exceeds maximum allowed size");
                    }
                    state_ = ParseState::BodyChunkData;
                }
                break;
            }

            case ParseState::BodyChunkData: {
                size_t needed = current_chunk_size_ - current_chunk_read_;
                size_t available = buffer_.size();
                size_t to_take = std::min(needed, available);

                message_.body.append(buffer_.substr(0, to_take));
                buffer_.erase(0, to_take);
                current_chunk_read_ += to_take;

                if (current_chunk_read_ >= current_chunk_size_) {
                    state_ = ParseState::BodyChunkCRLF;
                } else {
                    return true; // Need more data for current chunk
                }
                break;
            }

            case ParseState::BodyChunkCRLF: {
                if (buffer_.empty()) {
                    return true;
                }
                if (buffer_.front() == '\r') {
                    if (buffer_.size() < 2) {
                        return true; // Wait for following '\n'
                    }
                    if (buffer_[1] == '\n') {
                        buffer_.erase(0, 2);
                    } else {
                        return fail(400, "Missing LF after CR following chunk data");
                    }
                } else if (buffer_.front() == '\n') {
                    buffer_.erase(0, 1);
                } else {
                    return fail(400, "Missing CRLF delimiter after chunk data");
                }
                state_ = ParseState::BodyChunkSize;
                break;
            }

            case ParseState::Trailers: {
                while (true) {
                    size_t line_end = find_crlf(buffer_, 0);
                    if (line_end == std::string::npos) {
                        return true; // Need more data
                    }

                    if (line_end == 0 || (line_end == 1 && buffer_[0] == '\r')) {
                        // Terminal empty line
                        size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                        buffer_.erase(0, line_end + crlf_len);
                        state_ = ParseState::Complete;
                        return true;
                    }

                    std::string trailer_line = buffer_.substr(0, line_end);
                    size_t crlf_len = (buffer_[line_end] == '\r') ? 2 : 1;
                    buffer_.erase(0, line_end + crlf_len);

                    if (!parse_header_line(trailer_line)) {
                        return false;
                    }
                }
            }

            case ParseState::Complete:
            case ParseState::Error:
                return state_ == ParseState::Complete;
            }
        }
    }

private:
    [[nodiscard]] size_t find_crlf(std::string_view s, size_t start) const noexcept {
        size_t n = s.find('\n', start);
        if (n == std::string_view::npos) {
            return std::string_view::npos;
        }
        if (n > 0 && s[n - 1] == '\r') {
            return n - 1;
        }
        return n;
    }

    bool fail(int code, std::string message) {
        state_ = ParseState::Error;
        error_code_ = code;
        error_msg_ = std::move(message);
        return false;
    }

    bool parse_request_line(std::string_view line) {
        line = detail::trim_ascii(line);
        if (line.empty()) {
            return fail(400, "Empty HTTP request line");
        }

        size_t first_space = line.find(' ');
        if (first_space == std::string_view::npos) {
            return fail(400, "Malformed request line: missing method delimiter");
        }

        std::string method = std::string(line.substr(0, first_space));
        size_t second_space = line.find(' ', first_space + 1);
        if (second_space == std::string_view::npos) {
            return fail(400, "Malformed request line: missing protocol version delimiter");
        }

        std::string raw_target = std::string(line.substr(first_space + 1, second_space - (first_space + 1)));
        std::string version = std::string(line.substr(second_space + 1));

        if (version != "HTTP/1.1" && version != "HTTP/1.0") {
            return fail(505, "Unsupported HTTP version: " + version);
        }

        // Split target into path and query
        std::string path;
        std::string query_str;
        size_t qmark = raw_target.find('?');
        if (qmark != std::string::npos) {
            path = url_decode(raw_target.substr(0, qmark), false);
            query_str = raw_target.substr(qmark + 1);
        } else {
            path = url_decode(raw_target, false);
            query_str = "";
        }

        message_.line.method = std::move(method);
        message_.line.raw_target = raw_target;
        message_.line.path = std::move(path);
        message_.line.query_string = query_str;
        message_.line.version = std::move(version);
        message_.query = parse_query_string(query_str);

        return true;
    }

    bool parse_header_line(std::string_view line) {
        size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            return fail(400, "Malformed HTTP header line: missing colon");
        }

        std::string name = std::string(detail::trim_ascii(line.substr(0, colon)));
        std::string value = std::string(detail::trim_ascii(line.substr(colon + 1)));

        if (name.empty()) {
            return fail(400, "Empty HTTP header name");
        }

        message_.headers.add(std::move(name), std::move(value));
        return true;
    }

    bool transition_to_body() {
        // Inspect Connection header
        std::string conn = detail::to_lower_ascii(message_.headers.get_or("connection", ""));
        if (message_.line.version == "HTTP/1.0") {
            message_.keep_alive = (conn == "keep-alive");
        } else {
            message_.keep_alive = (conn != "close");
        }

        // Check WebSocket Upgrade headers
        std::string upgrade = detail::to_lower_ascii(message_.headers.get_or("upgrade", ""));
        if (upgrade == "websocket" && conn.find("upgrade") != std::string::npos) {
            message_.upgrade_websocket = true;
        }

        // Inspect Transfer-Encoding
        std::string transfer_enc = detail::to_lower_ascii(message_.headers.get_or("transfer-encoding", ""));
        if (transfer_enc.find("chunked") != std::string::npos) {
            is_chunked_ = true;
            state_ = ParseState::BodyChunkSize;
            return true;
        }

        // Inspect Content-Length
        auto cl_opt = message_.headers.get("content-length");
        if (cl_opt.has_value()) {
            std::string_view cl_str = detail::trim_ascii(*cl_opt);
            size_t cl = 0;
            auto [ptr, ec] = std::from_chars(cl_str.data(), cl_str.data() + cl_str.size(), cl);
            if (ec != std::errc{} || ptr != cl_str.data() + cl_str.size()) {
                return fail(400, "Invalid Content-Length header: " + std::string(cl_str));
            }
            if (cl > limits_.max_body_size) {
                return fail(413, "Content-Length exceeds maximum allowed payload size");
            }
            content_length_ = cl;
            body_bytes_read_ = 0;
            state_ = (content_length_ > 0) ? ParseState::BodyFixed : ParseState::Complete;
            return true;
        }

        // No Content-Length and not chunked -> request has no body
        state_ = ParseState::Complete;
        return true;
    }

    HttpParserLimits limits_;
    std::string buffer_;
    ParseState state_ = ParseState::RequestLine;
    HttpRequestMessage message_;
    int error_code_ = 200;
    std::string error_msg_;

    size_t content_length_ = 0;
    size_t body_bytes_read_ = 0;
    size_t current_chunk_size_ = 0;
    size_t current_chunk_read_ = 0;
    bool is_chunked_ = false;
};

// ============================================================================
// HTTP Response Formatter & Serializer
// ============================================================================

struct HttpResponseMessage {
    int status = 200;
    std::string reason;
    HeaderMap headers;
    std::string body;
    bool chunked = false;
};

class HttpResponseSerializer {
public:
    static std::string serialize(const HttpResponseMessage& resp, bool auto_headers = true) {
        std::string out;
        out.reserve(256 + resp.body.size());

        // Status line
        std::string_view reason = resp.reason.empty() ? status_reason_phrase(resp.status) : std::string_view(resp.reason);
        out += "HTTP/1.1 " + std::to_string(resp.status) + " " + std::string(reason) + "\r\n";

        // Create working copy of headers to merge defaults
        HeaderMap hdrs = resp.headers;

        if (auto_headers) {
            if (!hdrs.has("Date")) {
                hdrs.set("Date", format_http_date());
            }
            if (!hdrs.has("Server")) {
                hdrs.set("Server", "keety-http/1.0");
            }
            if (resp.chunked) {
                hdrs.set("Transfer-Encoding", "chunked");
            } else if (!hdrs.has("Content-Length")) {
                hdrs.set("Content-Length", std::to_string(resp.body.size()));
            }
            if (!hdrs.has("Content-Type") && !resp.body.empty()) {
                hdrs.set("Content-Type", "text/plain; charset=utf-8");
            }
        }

        // Serialize headers
        for (const auto& [name, value] : hdrs.entries()) {
            out += name + ": " + value + "\r\n";
        }

        out += "\r\n";

        // Serialize body
        if (!resp.chunked) {
            out.append(resp.body);
        }

        return out;
    }

    [[nodiscard]] static std::string format_chunk(std::string_view data) {
        if (data.empty()) {
            return "";
        }
        std::ostringstream ss;
        ss << std::hex << data.size() << "\r\n";
        std::string prefix = ss.str();
        std::string chunk;
        chunk.reserve(prefix.size() + data.size() + 2);
        chunk.append(prefix);
        chunk.append(data);
        chunk.append("\r\n");
        return chunk;
    }

    [[nodiscard]] static std::string format_last_chunk() {
        return "0\r\n\r\n";
    }
};

// ============================================================================
// Phase 2.2: Express-Style Middleware Pipeline & Fluent Request/Response API
// ============================================================================

struct CookieOptions {
    int64_t max_age = -1; ///< Max-Age in seconds (-1 = session cookie)
    std::string domain;
    std::string path = "/";
    bool secure = false;
    bool http_only = false;
    enum class SameSite { None, Lax, Strict } same_site = SameSite::Lax;
    std::string expires;
};

namespace detail {

inline std::unordered_map<std::string, std::string> parse_cookies(std::string_view cookie_hdr) {
    std::unordered_map<std::string, std::string> cookies;
    size_t start = 0;
    while (start < cookie_hdr.size()) {
        size_t semi = cookie_hdr.find(';', start);
        std::string_view pair = (semi == std::string_view::npos)
            ? cookie_hdr.substr(start)
            : cookie_hdr.substr(start, semi - start);
        pair = trim_ascii(pair);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            if (eq != std::string_view::npos) {
                std::string name = std::string(trim_ascii(pair.substr(0, eq)));
                std::string val = std::string(trim_ascii(pair.substr(eq + 1)));
                if (!name.empty()) {
                    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                        val = val.substr(1, val.size() - 2);
                    }
                    cookies[name] = url_decode(val);
                }
            }
        }
        if (semi == std::string_view::npos) break;
        start = semi + 1;
    }
    return cookies;
}

inline std::string format_set_cookie(std::string_view name, std::string_view value, const CookieOptions& options = {}) {
    std::string out = std::string(name) + "=" + std::string(value);
    if (!options.path.empty()) {
        out += "; Path=" + options.path;
    }
    if (!options.domain.empty()) {
        out += "; Domain=" + options.domain;
    }
    if (options.max_age >= 0) {
        out += "; Max-Age=" + std::to_string(options.max_age);
    }
    if (!options.expires.empty()) {
        out += "; Expires=" + options.expires;
    }
    if (options.secure) {
        out += "; Secure";
    }
    if (options.http_only) {
        out += "; HttpOnly";
    }
    switch (options.same_site) {
        case CookieOptions::SameSite::None:
            out += "; SameSite=None";
            break;
        case CookieOptions::SameSite::Strict:
            out += "; SameSite=Strict";
            break;
        case CookieOptions::SameSite::Lax:
        default:
            out += "; SameSite=Lax";
            break;
    }
    return out;
}

inline bool path_starts_with_prefix(std::string_view path, std::string_view prefix) {
    if (prefix.empty() || prefix == "/") {
        return true;
    }
    while (!prefix.empty() && prefix.back() == '/') {
        prefix.remove_suffix(1);
    }
    if (path.size() < prefix.size()) {
        return false;
    }
    if (path.substr(0, prefix.size()) != prefix) {
        return false;
    }
    return path.size() == prefix.size() || path[prefix.size()] == '/';
}

} // namespace detail

class Request {
public:
    std::string method;
    std::string url;
    std::string path;
    std::string protocol = "HTTP/1.1";
    std::string ip = "127.0.0.1";
    int port = 0;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query;
    HeaderMap headers;
    std::unordered_map<std::string, std::string> cookies;
    std::string body;
    bool keep_alive = true;
    bool upgrade_websocket = false;

    std::unordered_map<std::string, std::string> locals;
    std::unordered_map<std::string, std::string> body_params;
    std::string raw_body;
    std::shared_ptr<keet::json::JsonNode> json_value;

    Request() = default;
    explicit Request(const HttpRequestMessage& msg)
        : method(msg.line.method),
          url(msg.line.raw_target),
          path(msg.line.path),
          protocol(msg.line.version),
          ip(msg.client_ip),
          port(msg.client_port),
          params(msg.params),
          query(msg.query),
          headers(msg.headers),
          cookies(),
          body(msg.body),
          keep_alive(msg.keep_alive),
          upgrade_websocket(msg.upgrade_websocket),
          locals(msg.locals),
          body_params(msg.body_params),
          raw_body(msg.raw_body),
          json_value(msg.json_value) {
        auto cookie_hdr = headers.get("cookie");
        if (cookie_hdr.has_value()) {
            cookies = detail::parse_cookies(*cookie_hdr);
        }
    }

    [[nodiscard]] std::string header(std::string_view name, std::string_view fallback = "") const {
        return headers.get_or(name, fallback);
    }

    [[nodiscard]] std::string param(std::string_view name, std::string_view fallback = "") const {
        auto it = params.find(std::string(name));
        return (it != params.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string query_param(std::string_view name, std::string_view fallback = "") const {
        auto it = query.find(std::string(name));
        return (it != query.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string body_param(std::string_view name, std::string_view fallback = "") const {
        auto it = body_params.find(std::string(name));
        return (it != body_params.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string cookie(std::string_view name, std::string_view fallback = "") const {
        auto it = cookies.find(std::string(name));
        return (it != cookies.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] bool has_json() const noexcept {
        return json_value != nullptr;
    }

    [[nodiscard]] const keet::json::JsonNode* json_opt() const noexcept {
        return json_value.get();
    }

    [[nodiscard]] const keet::json::JsonNode& json() const {
        if (!json_value) {
            throw keet::error("HttpError", "Request does not contain a parsed JSON body");
        }
        return *json_value;
    }

    [[nodiscard]] std::string json_field(std::string_view key, std::string_view fallback = "") const {
        if (!json_value || !json_value->is_object()) {
            return std::string(fallback);
        }
        const auto& obj = std::get<keet::json::JsonNode::Object>(json_value->value);
        auto it = obj.find(std::string(key));
        if (it == obj.end()) {
            return std::string(fallback);
        }
        if (it->second.is_string()) {
            return std::get<std::string>(it->second.value);
        }
        return keet::json::detail::dump_node(it->second, 0);
    }

    [[nodiscard]] std::string validated(std::string_view name, std::string_view fallback = "") const {
        auto it = locals.find(std::string(name));
        if (it != locals.end()) return it->second;
        it = locals.find("validated." + std::string(name));
        if (it != locals.end()) return it->second;
        it = locals.find("body." + std::string(name));
        if (it != locals.end()) return it->second;
        it = locals.find("query." + std::string(name));
        if (it != locals.end()) return it->second;
        it = locals.find("params." + std::string(name));
        if (it != locals.end()) return it->second;
        return std::string(fallback);
    }

    [[nodiscard]] int64_t validated_int(std::string_view name, int64_t fallback = 0) const {
        std::string s = validated(name);
        if (s.empty()) return fallback;
        int64_t val = 0;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
        if (ec == std::errc{} && ptr == s.data() + s.size()) return val;
        return fallback;
    }

    [[nodiscard]] double validated_float(std::string_view name, double fallback = 0.0) const {
        std::string s = validated(name);
        if (s.empty()) return fallback;
        try {
            size_t idx = 0;
            double val = std::stod(s, &idx);
            if (idx == s.size()) return val;
        } catch (...) {}
        return fallback;
    }

    [[nodiscard]] bool validated_bool(std::string_view name, bool fallback = false) const {
        std::string s = validated(name);
        if (s.empty()) return fallback;
        std::string l = detail::to_lower_ascii(s);
        if (l == "true" || l == "1" || l == "yes") return true;
        if (l == "false" || l == "0" || l == "no") return false;
        return fallback;
    }

    [[nodiscard]] HttpRequestMessage to_raw_message() const {
        HttpRequestMessage msg;
        msg.line.method = method;
        msg.line.raw_target = url;
        msg.line.path = path;
        msg.line.version = protocol;
        msg.client_ip = ip;
        msg.client_port = port;
        msg.params = params;
        msg.query = query;
        msg.locals = locals;
        msg.body_params = body_params;
        msg.raw_body = raw_body;
        msg.json_value = json_value;
        msg.headers = headers;
        msg.body = body;
        msg.keep_alive = keep_alive;
        msg.upgrade_websocket = upgrade_websocket;
        return msg;
    }
};

class Response {
public:
    int status_code = 200;
    HeaderMap headers;
    std::string body;
    bool chunked = false;

    Response() = default;

    Response& status(int code) {
        status_code = code;
        return *this;
    }

    Response& setHeader(std::string_view name, std::string_view value) {
        headers.set(name, value);
        return *this;
    }

    Response& header(std::string_view name, std::string_view value) {
        return setHeader(name, value);
    }

    Response& appendHeader(std::string_view name, std::string_view value) {
        headers.add(std::string(name), std::string(value));
        return *this;
    }

    Response& type(std::string_view content_type) {
        headers.set("Content-Type", content_type);
        return *this;
    }

    Response& cookie(std::string_view name, std::string_view value, const CookieOptions& options = {}) {
        std::string set_cookie_val = detail::format_set_cookie(name, value, options);
        headers.add("Set-Cookie", set_cookie_val);
        return *this;
    }

    Response& clearCookie(std::string_view name, std::string_view path = "/") {
        CookieOptions opt;
        opt.path = std::string(path);
        opt.max_age = 0;
        opt.expires = "Thu, 01 Jan 1970 00:00:00 GMT";
        return cookie(name, "", opt);
    }

    Response& send(std::string_view text) {
        if (!headers.has("Content-Type")) {
            headers.set("Content-Type", "text/plain; charset=utf-8");
        }
        body = std::string(text);
        sent_ = true;
        return *this;
    }

    Response& json(std::string_view json_str) {
        headers.set("Content-Type", "application/json; charset=utf-8");
        body = std::string(json_str);
        sent_ = true;
        return *this;
    }

    Response& json(const keet::json::JsonNode& node) {
        return json(keet::json::detail::dump_node(node, 0));
    }

    Response& html(std::string_view html_str) {
        headers.set("Content-Type", "text/html; charset=utf-8");
        body = std::string(html_str);
        sent_ = true;
        return *this;
    }

    Response& redirect(std::string_view location, int redirect_code = 302) {
        status(redirect_code);
        setHeader("Location", location);
        if (body.empty()) {
            body = "Redirecting to " + std::string(location);
        }
        sent_ = true;
        return *this;
    }

    Response& end() {
        sent_ = true;
        return *this;
    }

    [[nodiscard]] bool is_sent() const noexcept {
        return sent_;
    }

    [[nodiscard]] HttpResponseMessage to_message() const {
        HttpResponseMessage msg;
        msg.status = status_code;
        msg.headers = headers;
        msg.body = body;
        msg.chunked = chunked;
        return msg;
    }

private:
    bool sent_ = false;
};

class NextFunction {
public:
    using Fn = std::function<void(std::optional<std::string>)>;
    NextFunction() = default;
    NextFunction(Fn fn) : fn_(std::move(fn)) {}

    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, NextFunction> && std::is_invocable_v<F, std::optional<std::string>>>>
    NextFunction(F&& fn) : fn_(std::forward<F>(fn)) {}

    void operator()() const {
        if (fn_) fn_(std::nullopt);
    }

    void operator()(const char* err) const {
        if (fn_) fn_(err ? std::optional<std::string>(std::string(err)) : std::nullopt);
    }

    void operator()(std::string_view err) const {
        if (fn_) fn_(std::string(err));
    }

    void operator()(const std::string& err) const {
        if (fn_) fn_(err);
    }

    void operator()(std::optional<std::string> err) const {
        if (fn_) fn_(std::move(err));
    }

    void operator()(std::nullopt_t) const {
        if (fn_) fn_(std::nullopt);
    }

private:
    Fn fn_;
};

using Middleware = std::function<void(Request&, Response&, NextFunction)>;
using Handler = std::function<void(Request&, Response&)>;
using ErrorMiddleware = std::function<void(const std::string&, Request&, Response&, NextFunction)>;

struct MiddlewareEntry {
    std::string prefix;
    bool is_error_handler = false;
    Middleware handler;
    ErrorMiddleware error_handler;
};

// ============================================================================
// Phase 3.1: Standard Middleware Ecosystem
// ============================================================================

namespace detail {

inline std::string mime_type_for_path(std::string_view ext) {
    std::string lower = detail::to_lower_ascii(ext);
    if (lower == ".html" || lower == ".htm") return "text/html; charset=utf-8";
    if (lower == ".css") return "text/css; charset=utf-8";
    if (lower == ".js" || lower == ".mjs") return "application/javascript; charset=utf-8";
    if (lower == ".json") return "application/json; charset=utf-8";
    if (lower == ".png") return "image/png";
    if (lower == ".jpg" || lower == ".jpeg") return "image/jpeg";
    if (lower == ".gif") return "image/gif";
    if (lower == ".svg") return "image/svg+xml";
    if (lower == ".ico") return "image/x-icon";
    if (lower == ".txt") return "text/plain; charset=utf-8";
    if (lower == ".pdf") return "application/pdf";
    if (lower == ".wasm") return "application/wasm";
    return "application/octet-stream";
}

} // namespace detail

struct CorsOptions {
    std::string origin = "*";
    std::vector<std::string> methods = {"GET", "HEAD", "PUT", "PATCH", "POST", "DELETE"};
    std::vector<std::string> allowed_headers;
    std::vector<std::string> exposed_headers;
    bool credentials = false;
    int max_age = 0;
};

inline Middleware cors(CorsOptions options = {}) {
    return [opts = std::move(options)](Request& req, Response& res, NextFunction next) {
        std::string req_origin = req.header("origin");
        if (!req_origin.empty()) {
            if (opts.origin == "*") {
                res.setHeader("Access-Control-Allow-Origin", opts.credentials ? req_origin : "*");
            } else {
                bool allowed = false;
                if (opts.origin == req_origin) {
                    allowed = true;
                } else {
                    size_t start = 0;
                    while (start < opts.origin.size()) {
                        size_t comma = opts.origin.find(',', start);
                        std::string_view o = (comma == std::string_view::npos)
                            ? std::string_view(opts.origin).substr(start)
                            : std::string_view(opts.origin).substr(start, comma - start);
                        o = detail::trim_ascii(o);
                        if (o == req_origin) {
                            allowed = true;
                            break;
                        }
                        if (comma == std::string_view::npos) break;
                        start = comma + 1;
                    }
                }
                if (allowed) {
                    res.setHeader("Access-Control-Allow-Origin", req_origin);
                }
            }
        } else if (opts.origin == "*") {
            res.setHeader("Access-Control-Allow-Origin", "*");
        }

        if (opts.origin != "*") {
            res.appendHeader("Vary", "Origin");
        }

        if (opts.credentials) {
            res.setHeader("Access-Control-Allow-Credentials", "true");
        }

        if (!opts.exposed_headers.empty()) {
            std::string exp;
            for (size_t i = 0; i < opts.exposed_headers.size(); ++i) {
                if (i > 0) exp += ", ";
                exp += opts.exposed_headers[i];
            }
            res.setHeader("Access-Control-Expose-Headers", exp);
        }

        if (detail::to_upper_ascii(req.method) == "OPTIONS" && req.headers.has("Access-Control-Request-Method")) {
            std::string methods_hdr;
            for (size_t i = 0; i < opts.methods.size(); ++i) {
                if (i > 0) methods_hdr += ", ";
                methods_hdr += opts.methods[i];
            }
            res.setHeader("Access-Control-Allow-Methods", methods_hdr);

            if (!opts.allowed_headers.empty()) {
                std::string hdrs;
                for (size_t i = 0; i < opts.allowed_headers.size(); ++i) {
                    if (i > 0) hdrs += ", ";
                    hdrs += opts.allowed_headers[i];
                }
                res.setHeader("Access-Control-Allow-Headers", hdrs);
            } else if (req.headers.has("Access-Control-Request-Headers")) {
                res.setHeader("Access-Control-Allow-Headers", req.header("access-control-request-headers"));
            }

            if (opts.max_age > 0) {
                res.setHeader("Access-Control-Max-Age", std::to_string(opts.max_age));
            }

            res.status(204);
            res.setHeader("Content-Length", "0");
            res.end();
            return;
        }

        next();
    };
}

inline Middleware jsonParser(size_t max_body_size = 10 * 1024 * 1024) {
    return [max_body_size](Request& req, Response& res, NextFunction next) {
        if (req.body.empty()) {
            next();
            return;
        }

        std::string ct = detail::to_lower_ascii(req.header("content-type"));
        if (ct.find("application/json") == std::string::npos) {
            next();
            return;
        }

        if (req.body.size() > max_body_size) {
            res.status(413).type("application/json").send("{\"error\":\"Payload Too Large\"}");
            return;
        }

        try {
            req.json_value = std::make_shared<keet::json::JsonNode>(keet::json::parse(req.body));
            next();
        } catch (const std::exception& ex) {
            res.status(400).type("application/json").send("{\"error\":\"Bad Request\",\"message\":\"" + keet::json::detail::escape_json_string(ex.what()) + "\"}");
        }
    };
}

inline Middleware urlencodedParser(size_t max_body_size = 10 * 1024 * 1024) {
    return [max_body_size](Request& req, Response& res, NextFunction next) {
        if (req.body.empty()) {
            next();
            return;
        }

        std::string ct = detail::to_lower_ascii(req.header("content-type"));
        if (ct.find("application/x-www-form-urlencoded") == std::string::npos) {
            next();
            return;
        }

        if (req.body.size() > max_body_size) {
            res.status(413).send("Payload Too Large");
            return;
        }

        req.body_params = parse_query_string(req.body);
        next();
    };
}

inline Middleware rawParser(size_t max_body_size = 50 * 1024 * 1024) {
    return [max_body_size](Request& req, Response& res, NextFunction next) {
        if (req.body.size() > max_body_size) {
            res.status(413).send("Payload Too Large");
            return;
        }
        req.raw_body = req.body;
        next();
    };
}

struct LoggerOptions {
    std::function<void(std::string_view)> sink;
    bool color = true;
};

inline Middleware logger(LoggerOptions options = {}) {
    return [opts = std::move(options)](Request& req, Response& res, NextFunction next) {
        auto t0 = std::chrono::steady_clock::now();
        next();
        auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

        std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm_buf{};
        detail::safe_gmtime(&now_c, &tm_buf);
        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

        std::ostringstream ss;
        ss << "[" << time_str << "] " << req.method << " " << req.path << " ";

        int st = res.status_code;
        if (opts.color) {
            if (st >= 200 && st < 300) {
                ss << "\033[32m" << st << "\033[0m";
            } else if (st >= 300 && st < 400) {
                ss << "\033[36m" << st << "\033[0m";
            } else if (st >= 400 && st < 500) {
                ss << "\033[33m" << st << "\033[0m";
            } else {
                ss << "\033[31m" << st << "\033[0m";
            }
        } else {
            ss << st;
        }

        ss << " - " << std::fixed << std::setprecision(2) << elapsed << "ms";
        std::string line = ss.str();

        if (opts.sink) {
            opts.sink(line);
        } else {
            std::cout << line << std::endl;
        }
    };
}

inline Middleware staticDir(std::string_view mountPath, std::string_view diskDir) {
    std::string mount_prefix = std::string(mountPath);
    while (!mount_prefix.empty() && mount_prefix.back() == '/') {
        mount_prefix.pop_back();
    }
    std::filesystem::path root(diskDir);

    return [mount_prefix, root](Request& req, Response& res, NextFunction next) {
        if (!mount_prefix.empty() && !detail::path_starts_with_prefix(req.path, mount_prefix)) {
            next();
            return;
        }

        std::string rel_path;
        if (!mount_prefix.empty()) {
            rel_path = req.path.substr(mount_prefix.size());
        } else {
            rel_path = req.path;
        }

        if (rel_path.empty() || rel_path == "/") {
            rel_path = "index.html";
        } else if (rel_path.front() == '/') {
            rel_path.erase(0, 1);
        }

        if (rel_path.find("..") != std::string::npos || rel_path.find('\\') != std::string::npos) {
            res.status(403).send("Forbidden");
            return;
        }

        std::filesystem::path full_path = root / rel_path;
        std::error_code ec;
        if (!std::filesystem::exists(full_path, ec) || !std::filesystem::is_regular_file(full_path, ec)) {
            next();
            return;
        }

        auto canon_root = std::filesystem::weakly_canonical(root, ec);
        auto canon_full = std::filesystem::weakly_canonical(full_path, ec);
        if (canon_full.string().rfind(canon_root.string(), 0) != 0) {
            res.status(403).send("Forbidden");
            return;
        }

        auto file_size = std::filesystem::file_size(full_path, ec);
        auto lwt = std::filesystem::last_write_time(full_path, ec);
        auto sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            lwt - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::string last_mod = format_http_date(sys_time);
        auto lwt_secs = std::chrono::duration_cast<std::chrono::seconds>(sys_time.time_since_epoch()).count();
        std::string etag = "\"" + std::to_string(file_size) + "-" + std::to_string(lwt_secs) + "\"";

        if (req.header("if-none-match") == etag) {
            res.status(304);
            res.setHeader("ETag", etag);
            res.end();
            return;
        }
        if (!last_mod.empty() && req.header("if-modified-since") == last_mod) {
            res.status(304);
            res.setHeader("Last-Modified", last_mod);
            res.end();
            return;
        }

        std::ifstream ifs(full_path, std::ios::in | std::ios::binary);
        if (!ifs.is_open()) {
            next();
            return;
        }
        std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string ext = full_path.extension().string();
        std::string mime = detail::mime_type_for_path(ext);

        res.status(200);
        res.setHeader("Content-Type", mime);
        res.setHeader("Last-Modified", last_mod);
        res.setHeader("ETag", etag);
        res.send(data);
    };
}

inline Middleware recover() {
    return [](Request&, Response& res, NextFunction next) {
        try {
            next();
        } catch (const std::exception& ex) {
            if (!res.is_sent()) {
                res.status(500);
                res.setHeader("Content-Type", "application/json; charset=utf-8");
                res.send("{\"error\":\"Internal Server Error\",\"message\":\"" + keet::json::detail::escape_json_string(ex.what()) + "\"}");
            }
        } catch (...) {
            if (!res.is_sent()) {
                res.status(500);
                res.setHeader("Content-Type", "application/json; charset=utf-8");
                res.send("{\"error\":\"Internal Server Error\"}");
            }
        }
    };
}

// ============================================================================
// Phase 3.2: Declarative Schema Validation Engine
// ============================================================================

enum class SchemaType {
    String,
    Int,
    Float,
    Bool,
    Array,
    Object
};

using FieldType = SchemaType;

enum class SchemaTarget {
    Body,
    Query,
    Params,
    Headers
};

using Target = SchemaTarget;

class SchemaField {
public:
    std::string name;
    SchemaTarget target = SchemaTarget::Body;
    SchemaType type = SchemaType::String;
    bool is_required = true;
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::optional<std::string> pattern_regex;
    std::vector<std::string> enum_list;
    std::optional<std::string> default_value_str;
    std::string custom_message;

    SchemaField() = default;

    explicit SchemaField(std::string field_name, SchemaType field_type = SchemaType::String, SchemaTarget tgt = SchemaTarget::Body)
        : name(std::move(field_name)), target(tgt), type(field_type) {}

    SchemaField& required(bool req = true) {
        is_required = req;
        return *this;
    }

    SchemaField& optional() {
        is_required = false;
        return *this;
    }

    SchemaField& not_empty() {
        min_value = 1.0;
        return *this;
    }

    SchemaField& min(double min_val) {
        min_value = min_val;
        return *this;
    }

    SchemaField& max(double max_val) {
        max_value = max_val;
        return *this;
    }

    SchemaField& pattern(std::string_view regex_str) {
        pattern_regex = std::string(regex_str);
        return *this;
    }

    SchemaField& enum_values(std::vector<std::string> allowed) {
        enum_list = std::move(allowed);
        return *this;
    }

    SchemaField& enum_values(std::initializer_list<std::string> allowed) {
        enum_list = std::vector<std::string>(allowed);
        return *this;
    }

    SchemaField& default_value(std::string_view def) {
        default_value_str = std::string(def);
        is_required = false;
        return *this;
    }

    SchemaField& message(std::string_view msg) {
        custom_message = std::string(msg);
        return *this;
    }
};

class Schema {
public:
    std::vector<SchemaField> fields;

    Schema() = default;

    SchemaField& body(std::string_view name, SchemaType type = SchemaType::String) {
        fields.emplace_back(std::string(name), type, SchemaTarget::Body);
        return fields.back();
    }

    SchemaField& query(std::string_view name, SchemaType type = SchemaType::String) {
        fields.emplace_back(std::string(name), type, SchemaTarget::Query);
        return fields.back();
    }

    SchemaField& param(std::string_view name, SchemaType type = SchemaType::String) {
        fields.emplace_back(std::string(name), type, SchemaTarget::Params);
        return fields.back();
    }

    SchemaField& header(std::string_view name, SchemaType type = SchemaType::String) {
        fields.emplace_back(std::string(name), type, SchemaTarget::Headers);
        return fields.back();
    }

    Schema& add(SchemaField f) {
        fields.push_back(std::move(f));
        return *this;
    }

    Schema& add_body(SchemaField f) {
        f.target = SchemaTarget::Body;
        fields.push_back(std::move(f));
        return *this;
    }

    Schema& add_query(SchemaField f) {
        f.target = SchemaTarget::Query;
        fields.push_back(std::move(f));
        return *this;
    }

    Schema& add_param(SchemaField f) {
        f.target = SchemaTarget::Params;
        fields.push_back(std::move(f));
        return *this;
    }

    Schema& add_header(SchemaField f) {
        f.target = SchemaTarget::Headers;
        fields.push_back(std::move(f));
        return *this;
    }
};

inline Schema schema() {
    return Schema{};
}

inline SchemaField field(std::string_view name, SchemaType type = SchemaType::String, SchemaTarget target = SchemaTarget::Body) {
    return SchemaField(std::string(name), type, target);
}

namespace detail {

inline std::string format_validation_number(double v) {
    if (std::floor(v) == v) {
        return std::to_string(static_cast<int64_t>(v));
    }
    std::ostringstream ss;
    ss << v;
    return ss.str();
}

inline std::string make_full_field_name(SchemaTarget target, std::string_view name) {
    std::string prefix;
    switch (target) {
        case SchemaTarget::Body: prefix = "body"; break;
        case SchemaTarget::Query: prefix = "query"; break;
        case SchemaTarget::Params: prefix = "params"; break;
        case SchemaTarget::Headers: prefix = "headers"; break;
    }
    if (name.rfind(prefix + ".", 0) == 0) {
        return std::string(name);
    }
    return prefix + "." + std::string(name);
}

inline const keet::json::JsonNode* find_json_path(const keet::json::JsonNode* root, std::string_view path) {
    if (!root) return nullptr;
    size_t start = 0;
    const keet::json::JsonNode* curr = root;
    while (start < path.size()) {
        size_t dot = path.find('.', start);
        std::string_view key = (dot == std::string_view::npos)
            ? path.substr(start)
            : path.substr(start, dot - start);
        if (!curr->is_object()) {
            return nullptr;
        }
        const auto& obj = std::get<keet::json::JsonNode::Object>(curr->value);
        auto it = obj.find(std::string(key));
        if (it == obj.end()) {
            return nullptr;
        }
        curr = &(it->second);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return curr;
}

inline void set_json_default(keet::json::JsonNode* root, std::string_view path, SchemaType type, std::string_view def_str) {
    if (!root || !root->is_object()) return;
    keet::json::JsonNode val_node;
    if (type == SchemaType::Int) {
        int64_t v = 0;
        std::from_chars(def_str.data(), def_str.data() + def_str.size(), v);
        val_node = keet::json::JsonNode(v);
    } else if (type == SchemaType::Float) {
        double v = 0.0;
        try { v = std::stod(std::string(def_str)); } catch (...) {}
        val_node = keet::json::JsonNode(v);
    } else if (type == SchemaType::Bool) {
        std::string l = to_lower_ascii(def_str);
        bool b = (l == "true" || l == "1" || l == "yes");
        val_node = keet::json::JsonNode(b);
    } else if (type == SchemaType::String) {
        val_node = keet::json::JsonNode(std::string(def_str));
    } else {
        try {
            val_node = keet::json::parse(def_str);
        } catch (...) {
            val_node = keet::json::JsonNode(std::string(def_str));
        }
    }

    auto& obj = std::get<keet::json::JsonNode::Object>(root->value);
    size_t dot = path.find('.');
    if (dot == std::string_view::npos) {
        obj[std::string(path)] = std::move(val_node);
    }
}

inline void fail_validation(Response& res, std::string_view field_full_name, std::string_view message) {
    res.status(400);
    res.setHeader("Content-Type", "application/json; charset=utf-8");
    std::string json_body = "{\"error\":\"ValidationError\",\"field\":" +
        keet::json::detail::escape_json_string(field_full_name) +
        ",\"message\":" +
        keet::json::detail::escape_json_string(message) +
        "}";
    res.send(json_body);
}

} // namespace detail

inline Middleware validate(Schema schema) {
    return [sch = std::move(schema)](Request& req, Response& res, NextFunction next) {
        for (const auto& field : sch.fields) {
            std::string full_name = detail::make_full_field_name(field.target, field.name);

            if (field.target == SchemaTarget::Query) {
                auto it = req.query.find(field.name);
                std::string raw_val;
                bool is_present = false;
                if (it != req.query.end()) {
                    raw_val = it->second;
                    is_present = true;
                }

                if (!is_present) {
                    if (field.default_value_str.has_value()) {
                        raw_val = *field.default_value_str;
                        req.query[field.name] = raw_val;
                        is_present = true;
                    } else if (field.is_required) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' is required")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    } else {
                        continue;
                    }
                }

                if (field.type == SchemaType::String) {
                    if (field.min_value.has_value() && raw_val.size() < static_cast<size_t>(*field.min_value)) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' length must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && raw_val.size() > static_cast<size_t>(*field.max_value)) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' length must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.pattern_regex.has_value()) {
                        try {
                            std::regex re(*field.pattern_regex);
                            if (!std::regex_search(raw_val, re)) {
                                std::string msg = field.custom_message.empty()
                                    ? ("Field '" + field.name + "' does not match required pattern")
                                    : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                        } catch (...) {
                            detail::fail_validation(res, full_name, "Invalid regex pattern for field '" + field.name + "'");
                            return;
                        }
                    }
                    if (!field.enum_list.empty()) {
                        if (std::find(field.enum_list.begin(), field.enum_list.end(), raw_val) == field.enum_list.end()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be one of allowed values")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    }
                } else if (field.type == SchemaType::Int) {
                    int64_t ival = 0;
                    auto [ptr, ec] = std::from_chars(raw_val.data(), raw_val.data() + raw_val.size(), ival);
                    if (ec != std::errc{} || ptr != raw_val.data() + raw_val.size()) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be an integer")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.min_value.has_value() && ival < *field.min_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && ival > *field.max_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (!field.enum_list.empty()) {
                        std::string is = std::to_string(ival);
                        if (std::find(field.enum_list.begin(), field.enum_list.end(), is) == field.enum_list.end()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be one of allowed values")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    }
                } else if (field.type == SchemaType::Float) {
                    double fval = 0.0;
                    size_t idx = 0;
                    try {
                        fval = std::stod(raw_val, &idx);
                        if (idx != raw_val.size()) {
                            throw std::runtime_error("invalid float");
                        }
                    } catch (...) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be a number")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.min_value.has_value() && fval < *field.min_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && fval > *field.max_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                } else if (field.type == SchemaType::Bool) {
                    std::string l = detail::to_lower_ascii(raw_val);
                    if (l == "true" || l == "1" || l == "yes") {
                        raw_val = "true";
                    } else if (l == "false" || l == "0" || l == "no") {
                        raw_val = "false";
                    } else {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be a boolean")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                }

                req.locals[field.name] = raw_val;
                req.locals[full_name] = raw_val;
                req.locals["validated." + field.name] = raw_val;
                req.locals["validated." + full_name] = raw_val;
            } else if (field.target == SchemaTarget::Params) {
                auto it = req.params.find(field.name);
                std::string raw_val;
                bool is_present = false;
                if (it != req.params.end()) {
                    raw_val = it->second;
                    is_present = true;
                }

                if (!is_present) {
                    if (field.default_value_str.has_value()) {
                        raw_val = *field.default_value_str;
                        req.params[field.name] = raw_val;
                        is_present = true;
                    } else if (field.is_required) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' is required")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    } else {
                        continue;
                    }
                }

                if (field.type == SchemaType::String) {
                    if (field.min_value.has_value() && raw_val.size() < static_cast<size_t>(*field.min_value)) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' length must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && raw_val.size() > static_cast<size_t>(*field.max_value)) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' length must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.pattern_regex.has_value()) {
                        try {
                            std::regex re(*field.pattern_regex);
                            if (!std::regex_search(raw_val, re)) {
                                std::string msg = field.custom_message.empty()
                                    ? ("Field '" + field.name + "' does not match required pattern")
                                    : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                        } catch (...) {
                            detail::fail_validation(res, full_name, "Invalid regex pattern for field '" + field.name + "'");
                            return;
                        }
                    }
                    if (!field.enum_list.empty()) {
                        if (std::find(field.enum_list.begin(), field.enum_list.end(), raw_val) == field.enum_list.end()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be one of allowed values")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    }
                } else if (field.type == SchemaType::Int) {
                    int64_t ival = 0;
                    auto [ptr, ec] = std::from_chars(raw_val.data(), raw_val.data() + raw_val.size(), ival);
                    if (ec != std::errc{} || ptr != raw_val.data() + raw_val.size()) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be an integer")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.min_value.has_value() && ival < *field.min_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && ival > *field.max_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (!field.enum_list.empty()) {
                        std::string is = std::to_string(ival);
                        if (std::find(field.enum_list.begin(), field.enum_list.end(), is) == field.enum_list.end()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be one of allowed values")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    }
                } else if (field.type == SchemaType::Float) {
                    double fval = 0.0;
                    size_t idx = 0;
                    try {
                        fval = std::stod(raw_val, &idx);
                        if (idx != raw_val.size()) {
                            throw std::runtime_error("invalid float");
                        }
                    } catch (...) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be a number")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.min_value.has_value() && fval < *field.min_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                    if (field.max_value.has_value() && fval > *field.max_value) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                } else if (field.type == SchemaType::Bool) {
                    std::string l = detail::to_lower_ascii(raw_val);
                    if (l == "true" || l == "1" || l == "yes") {
                        raw_val = "true";
                    } else if (l == "false" || l == "0" || l == "no") {
                        raw_val = "false";
                    } else {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' must be a boolean")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    }
                }

                req.locals[field.name] = raw_val;
                req.locals[full_name] = raw_val;
                req.locals["validated." + field.name] = raw_val;
                req.locals["validated." + full_name] = raw_val;
            } else if (field.target == SchemaTarget::Headers) {
                auto hdr_opt = req.headers.get(field.name);
                std::string raw_val;
                bool is_present = false;
                if (hdr_opt.has_value()) {
                    raw_val = *hdr_opt;
                    is_present = true;
                }

                if (!is_present) {
                    if (field.default_value_str.has_value()) {
                        raw_val = *field.default_value_str;
                        is_present = true;
                    } else if (field.is_required) {
                        std::string msg = field.custom_message.empty()
                            ? ("Field '" + field.name + "' is required")
                            : field.custom_message;
                        detail::fail_validation(res, full_name, msg);
                        return;
                    } else {
                        continue;
                    }
                }

                if (field.min_value.has_value() && raw_val.size() < static_cast<size_t>(*field.min_value)) {
                    std::string msg = field.custom_message.empty()
                        ? ("Field '" + field.name + "' length must be at least " + detail::format_validation_number(*field.min_value))
                        : field.custom_message;
                    detail::fail_validation(res, full_name, msg);
                    return;
                }
                if (field.max_value.has_value() && raw_val.size() > static_cast<size_t>(*field.max_value)) {
                    std::string msg = field.custom_message.empty()
                        ? ("Field '" + field.name + "' length must be at most " + detail::format_validation_number(*field.max_value))
                        : field.custom_message;
                    detail::fail_validation(res, full_name, msg);
                    return;
                }

                req.locals[field.name] = raw_val;
                req.locals[full_name] = raw_val;
                req.locals["validated." + field.name] = raw_val;
                req.locals["validated." + full_name] = raw_val;
            } else if (field.target == SchemaTarget::Body) {
                if (!req.has_json() && !req.body.empty()) {
                    std::string ct = detail::to_lower_ascii(req.header("content-type"));
                    if (ct.find("application/json") != std::string::npos || req.body.front() == '{' || req.body.front() == '[') {
                        try {
                            req.json_value = std::make_shared<keet::json::JsonNode>(keet::json::parse(req.body));
                        } catch (const std::exception& ex) {
                            if (ct.find("application/json") != std::string::npos) {
                                detail::fail_validation(res, "body", "Invalid JSON payload: " + std::string(ex.what()));
                                return;
                            }
                        }
                    }
                }

                if (req.has_json()) {
                    const keet::json::JsonNode* node = detail::find_json_path(req.json_value.get(), field.name);
                    if (node == nullptr || node->is_null()) {
                        if (field.default_value_str.has_value()) {
                            detail::set_json_default(req.json_value.get(), field.name, field.type, *field.default_value_str);
                            node = detail::find_json_path(req.json_value.get(), field.name);
                        }
                    }

                    if (node == nullptr || node->is_null()) {
                        if (field.is_required) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' is required")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        } else {
                            continue;
                        }
                    }

                    std::string str_val;
                    if (field.type == SchemaType::String) {
                        if (!node->is_string()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be a string")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        str_val = std::get<std::string>(node->value);
                        if (field.min_value.has_value() && str_val.size() < static_cast<size_t>(*field.min_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' length must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && str_val.size() > static_cast<size_t>(*field.max_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' length must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.pattern_regex.has_value()) {
                            try {
                                std::regex re(*field.pattern_regex);
                                if (!std::regex_search(str_val, re)) {
                                    std::string msg = field.custom_message.empty()
                                        ? ("Field '" + field.name + "' does not match required pattern")
                                        : field.custom_message;
                                    detail::fail_validation(res, full_name, msg);
                                    return;
                                }
                            } catch (...) {
                                detail::fail_validation(res, full_name, "Invalid regex pattern for field '" + field.name + "'");
                                return;
                            }
                        }
                        if (!field.enum_list.empty()) {
                            if (std::find(field.enum_list.begin(), field.enum_list.end(), str_val) == field.enum_list.end()) {
                                std::string msg = field.custom_message.empty()
                                    ? ("Field '" + field.name + "' must be one of allowed values")
                                    : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                        }
                    } else if (field.type == SchemaType::Int) {
                        int64_t ival = 0;
                        if (node->is_int()) {
                            ival = std::get<int64_t>(node->value);
                        } else if (node->is_float()) {
                            double f = std::get<double>(node->value);
                            if (std::floor(f) != f) {
                                std::string msg = field.custom_message.empty()
                                    ? ("Field '" + field.name + "' must be an integer")
                                    : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                            ival = static_cast<int64_t>(f);
                        } else {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be an integer")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }

                        if (field.min_value.has_value() && ival < *field.min_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && ival > *field.max_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (!field.enum_list.empty()) {
                            std::string is = std::to_string(ival);
                            if (std::find(field.enum_list.begin(), field.enum_list.end(), is) == field.enum_list.end()) {
                                std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be one of allowed values")
                                : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                        }
                        str_val = std::to_string(ival);
                    } else if (field.type == SchemaType::Float) {
                        double fval = 0.0;
                        if (node->is_float()) {
                            fval = std::get<double>(node->value);
                        } else if (node->is_int()) {
                            fval = static_cast<double>(std::get<int64_t>(node->value));
                        } else {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be a number")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }

                        if (field.min_value.has_value() && fval < *field.min_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && fval > *field.max_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        str_val = detail::format_validation_number(fval);
                    } else if (field.type == SchemaType::Bool) {
                        if (!node->is_bool()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be a boolean")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        bool b = std::get<bool>(node->value);
                        str_val = b ? "true" : "false";
                    } else if (field.type == SchemaType::Array) {
                        if (!node->is_array()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be an array")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        const auto& arr = std::get<keet::json::JsonNode::Array>(node->value);
                        if (field.min_value.has_value() && arr.size() < static_cast<size_t>(*field.min_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must have at least " + detail::format_validation_number(*field.min_value) + " items")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && arr.size() > static_cast<size_t>(*field.max_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must have at most " + detail::format_validation_number(*field.max_value) + " items")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        str_val = keet::json::detail::dump_node(*node, 0);
                    } else if (field.type == SchemaType::Object) {
                        if (!node->is_object()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be an object")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        str_val = keet::json::detail::dump_node(*node, 0);
                    }

                    req.locals[field.name] = str_val;
                    req.locals[full_name] = str_val;
                    req.locals["validated." + field.name] = str_val;
                    req.locals["validated." + full_name] = str_val;
                } else {
                    auto it = req.body_params.find(field.name);
                    std::string raw_val;
                    bool is_present = false;
                    if (it != req.body_params.end()) {
                        raw_val = it->second;
                        is_present = true;
                    }

                    if (!is_present) {
                        if (field.default_value_str.has_value()) {
                            raw_val = *field.default_value_str;
                            req.body_params[field.name] = raw_val;
                            is_present = true;
                        } else if (field.is_required) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' is required")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        } else {
                            continue;
                        }
                    }

                    if (field.type == SchemaType::String) {
                        if (field.min_value.has_value() && raw_val.size() < static_cast<size_t>(*field.min_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' length must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && raw_val.size() > static_cast<size_t>(*field.max_value)) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' length must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.pattern_regex.has_value()) {
                            try {
                                std::regex re(*field.pattern_regex);
                                if (!std::regex_search(raw_val, re)) {
                                    std::string msg = field.custom_message.empty()
                                        ? ("Field '" + field.name + "' does not match required pattern")
                                        : field.custom_message;
                                    detail::fail_validation(res, full_name, msg);
                                    return;
                                }
                            } catch (...) {
                                detail::fail_validation(res, full_name, "Invalid regex pattern for field '" + field.name + "'");
                                return;
                            }
                        }
                        if (!field.enum_list.empty()) {
                            if (std::find(field.enum_list.begin(), field.enum_list.end(), raw_val) == field.enum_list.end()) {
                                std::string msg = field.custom_message.empty()
                                    ? ("Field '" + field.name + "' must be one of allowed values")
                                    : field.custom_message;
                                detail::fail_validation(res, full_name, msg);
                                return;
                            }
                        }
                    } else if (field.type == SchemaType::Int) {
                        int64_t ival = 0;
                        auto [ptr, ec] = std::from_chars(raw_val.data(), raw_val.data() + raw_val.size(), ival);
                        if (ec != std::errc{} || ptr != raw_val.data() + raw_val.size()) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be an integer")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.min_value.has_value() && ival < *field.min_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && ival > *field.max_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    } else if (field.type == SchemaType::Float) {
                        double fval = 0.0;
                        size_t idx = 0;
                        try {
                            fval = std::stod(raw_val, &idx);
                            if (idx != raw_val.size()) {
                                throw std::runtime_error("invalid float");
                            }
                        } catch (...) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be a number")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.min_value.has_value() && fval < *field.min_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at least " + detail::format_validation_number(*field.min_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                        if (field.max_value.has_value() && fval > *field.max_value) {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be at most " + detail::format_validation_number(*field.max_value))
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    } else if (field.type == SchemaType::Bool) {
                        std::string l = detail::to_lower_ascii(raw_val);
                        if (l == "true" || l == "1" || l == "yes") {
                            raw_val = "true";
                        } else if (l == "false" || l == "0" || l == "no") {
                            raw_val = "false";
                        } else {
                            std::string msg = field.custom_message.empty()
                                ? ("Field '" + field.name + "' must be a boolean")
                                : field.custom_message;
                            detail::fail_validation(res, full_name, msg);
                            return;
                        }
                    }

                    req.locals[field.name] = raw_val;
                    req.locals[full_name] = raw_val;
                    req.locals["validated." + field.name] = raw_val;
                    req.locals["validated." + full_name] = raw_val;
                }
            }
        }

        next();
    };
}

// ============================================================================
// Part 4: WebSockets (RFC 6455)
// Phase 4.1: WebSocket Upgrade Handshake
// ============================================================================

inline constexpr std::string_view kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/// Computes RFC 6455 Sec-WebSocket-Accept token: Base64(SHA1(client_key + GUID))
inline std::string compute_websocket_accept(std::string_view client_key) {
    std::string concat = std::string(client_key) + std::string(kWebSocketGuid);
    std::string digest = keet::crypto::sha1Bytes(concat);
    return keet::crypto::toBase64(digest);
}

enum class WebSocketUpgradeStatus {
    Success,
    NotAnUpgrade,
    VersionMismatch,
    InvalidKey,
    BadRequest
};

struct WebSocketUpgradeValidation {
    WebSocketUpgradeStatus status = WebSocketUpgradeStatus::NotAnUpgrade;
    std::string client_key;
    std::string accept_key;
    std::string selected_protocol;
    std::string error_message;
};

inline bool is_websocket_upgrade(std::string_view method, const HeaderMap& headers) {
    if (detail::to_upper_ascii(method) != "GET") {
        return false;
    }
    std::string up = detail::to_lower_ascii(headers.get_or("upgrade", ""));
    if (up != "websocket") {
        return false;
    }
    std::string conn = detail::to_lower_ascii(headers.get_or("connection", ""));
    return conn.find("upgrade") != std::string::npos;
}

inline bool is_websocket_upgrade(const HttpRequestMessage& msg) {
    return is_websocket_upgrade(msg.line.method, msg.headers);
}

inline bool is_websocket_upgrade(const Request& req) {
    return is_websocket_upgrade(req.method, req.headers);
}

inline WebSocketUpgradeValidation validate_websocket_upgrade(
    const Request& req,
    const std::vector<std::string>& supported_subprotocols = {}
) {
    WebSocketUpgradeValidation result;

    if (!is_websocket_upgrade(req)) {
        result.status = WebSocketUpgradeStatus::NotAnUpgrade;
        result.error_message = "Not a WebSocket upgrade request (requires GET with Upgrade: websocket and Connection: Upgrade)";
        return result;
    }

    std::string version = req.header("sec-websocket-version");
    if (version != "13") {
        result.status = WebSocketUpgradeStatus::VersionMismatch;
        result.error_message = "Unsupported WebSocket version '" + version + "' (requires version 13)";
        return result;
    }

    std::string key = req.header("sec-websocket-key");
    if (key.empty()) {
        result.status = WebSocketUpgradeStatus::InvalidKey;
        result.error_message = "Missing Sec-WebSocket-Key header";
        return result;
    }

    try {
        std::string decoded_key = keet::crypto::fromBase64(key);
        if (decoded_key.size() != 16) {
            result.status = WebSocketUpgradeStatus::InvalidKey;
            result.error_message = "Sec-WebSocket-Key must decode to exactly 16 bytes (got " + std::to_string(decoded_key.size()) + ")";
            return result;
        }
    } catch (...) {
        result.status = WebSocketUpgradeStatus::InvalidKey;
        result.error_message = "Sec-WebSocket-Key is not valid Base64";
        return result;
    }

    result.client_key = key;
    result.accept_key = compute_websocket_accept(key);

    // Subprotocol negotiation (RFC 6455 Sec 1.9 / 4.2.2)
    std::string client_proto = req.header("sec-websocket-protocol");
    if (!client_proto.empty() && !supported_subprotocols.empty()) {
        size_t start = 0;
        while (start < client_proto.size()) {
            size_t comma = client_proto.find(',', start);
            std::string_view p = (comma == std::string_view::npos)
                ? std::string_view(client_proto).substr(start)
                : std::string_view(client_proto).substr(start, comma - start);
            p = detail::trim_ascii(p);
            for (const auto& sup : supported_subprotocols) {
                if (p == sup) {
                    result.selected_protocol = std::string(p);
                    break;
                }
            }
            if (!result.selected_protocol.empty()) break;
            if (comma == std::string_view::npos) break;
            start = comma + 1;
        }
    }

    result.status = WebSocketUpgradeStatus::Success;
    return result;
}

inline HttpResponseMessage make_websocket_upgrade_response(
    const Request& req,
    const std::vector<std::string>& supported_subprotocols = {}
) {
    auto val = validate_websocket_upgrade(req, supported_subprotocols);
    HttpResponseMessage resp;

    if (val.status == WebSocketUpgradeStatus::Success) {
        resp.status = 101;
        resp.headers.set("Upgrade", "websocket");
        resp.headers.set("Connection", "Upgrade");
        resp.headers.set("Sec-WebSocket-Accept", val.accept_key);
        if (!val.selected_protocol.empty()) {
            resp.headers.set("Sec-WebSocket-Protocol", val.selected_protocol);
        }
        return resp;
    }

    if (val.status == WebSocketUpgradeStatus::VersionMismatch) {
        resp.status = 426;
        resp.headers.set("Sec-WebSocket-Version", "13");
        resp.headers.set("Connection", "close");
        resp.headers.set("Content-Type", "text/plain; charset=utf-8");
        resp.body = "Upgrade Required: WebSocket version 13 required";
        return resp;
    }

    resp.status = 400;
    resp.headers.set("Connection", "close");
    resp.headers.set("Content-Type", "text/plain; charset=utf-8");
    resp.body = "Bad Request: " + val.error_message;
    return resp;
}

enum class WebSocketState {
    Connecting,
    Open,
    Closing,
    Closed
};

class WebSocket;

using WebSocketHandler = std::function<void(std::shared_ptr<WebSocket>)>;

struct WebSocketRouteEntry {
    std::vector<std::string> subprotocols;
    WebSocketHandler handler;
};

// ============================================================================
// Phase 1.2: Server Socket Lifecycle & Concurrency Engine
// ============================================================================

namespace detail {

inline void set_tcp_nodelay(net::detail::socket_t fd, bool enabled) noexcept {
    int flag = enabled ? 1 : 0;
#if defined(_WIN32)
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));
#else
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, static_cast<socklen_t>(sizeof(flag)));
#endif
}

inline void set_reuse_addr_port(net::detail::socket_t fd, bool reuse_port = true) noexcept {
    int one = 1;
#if defined(_WIN32)
    if (reuse_port) {
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
    } else {
        ::setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&one), sizeof(one));
    }
#else
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, static_cast<socklen_t>(sizeof(one)));
#if defined(SO_REUSEPORT)
    (void)reuse_port;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, static_cast<socklen_t>(sizeof(one)));
#else
    (void)reuse_port;
#endif
#endif
}

inline int socket_poll(net::detail::socket_t fd, short events, int timeout_ms) {
    if (fd == net::detail::kInvalidSocket) {
        return -1;
    }
#if defined(_WIN32)
    WSAPOLLFD pfd{};
    pfd.fd = fd;
    pfd.events = events;
    return ::WSAPoll(&pfd, 1, timeout_ms);
#else
    struct pollfd pfd{};
    pfd.fd = fd;
    pfd.events = events;
    return ::poll(&pfd, 1, timeout_ms);
#endif
}

inline bool socket_send_all(net::detail::socket_t fd, const char* data, size_t total, int timeout_ms = 10000) {
    size_t sent = 0;
    while (sent < total) {
        int pr = socket_poll(fd, POLLOUT, timeout_ms);
        if (pr <= 0) {
            return false;
        }
#if defined(_WIN32)
        int n = ::send(fd, data + sent, static_cast<int>(total - sent), 0);
#else
        int flags = net::detail::send_flags();
        ssize_t n = ::send(fd, data + sent, total - sent, flags);
#endif
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

inline ssize_t socket_recv(net::detail::socket_t fd, char* buf, size_t max_len) {
#if defined(_WIN32)
    int n = ::recv(fd, buf, static_cast<int>(max_len), 0);
    return static_cast<ssize_t>(n);
#else
    return ::recv(fd, buf, max_len, 0);
#endif
}

} // namespace detail

// ============================================================================
// Phase 4.2: WebSocket Framing Engine & Client Abstraction (RFC 6455)
// ============================================================================

enum class WebSocketOpcode : uint8_t {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA
};

[[nodiscard]] inline constexpr bool is_control_opcode(WebSocketOpcode op) noexcept {
    return (static_cast<uint8_t>(op) & 0x08) != 0;
}

/// Validates whether a byte sequence is well-formed UTF-8 according to RFC 3629.
[[nodiscard]] inline bool is_valid_utf8(std::string_view sv) noexcept {
    const auto* s = reinterpret_cast<const uint8_t*>(sv.data());
    size_t len = sv.size();
    size_t i = 0;
    while (i < len) {
        if (s[i] <= 0x7F) {
            i++;
        } else if ((s[i] & 0xE0) == 0xC0) {
            if (i + 1 >= len || (s[i + 1] & 0xC0) != 0x80) return false;
            if (s[i] < 0xC2) return false; // Overlong
            i += 2;
        } else if ((s[i] & 0xF0) == 0xE0) {
            if (i + 2 >= len || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80) return false;
            if (s[i] == 0xE0 && s[i + 1] < 0xA0) return false; // Overlong
            if (s[i] == 0xED && s[i + 1] >= 0xA0) return false; // UTF-16 surrogates
            i += 3;
        } else if ((s[i] & 0xF8) == 0xF0) {
            if (i + 3 >= len || (s[i + 1] & 0xC0) != 0x80 || (s[i + 2] & 0xC0) != 0x80 || (s[i + 3] & 0xC0) != 0x80) return false;
            if (s[i] == 0xF0 && s[i + 1] < 0x90) return false; // Overlong
            if (s[i] == 0xF4 && s[i + 1] > 0x8F) return false; // > U+10FFFF
            if (s[i] > 0xF4) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

/// Serializes a WebSocket frame according to RFC 6455 §5.2.
/// Server-to-client frames MUST NOT be masked (masked = false).
/// Client simulation / testing can set masked = true.
inline std::string serialize_websocket_frame(
    WebSocketOpcode opcode,
    std::string_view payload,
    bool masked = false,
    std::optional<std::array<uint8_t, 4>> custom_mask = std::nullopt,
    bool fin = true
) {
    std::string frame;
    frame.reserve(14 + payload.size());

    uint8_t b0 = (fin ? 0x80 : 0x00) | (static_cast<uint8_t>(opcode) & 0x0F);
    frame.push_back(static_cast<char>(b0));

    uint8_t b1 = masked ? 0x80 : 0x00;
    size_t len = payload.size();

    if (len <= 125) {
        b1 |= static_cast<uint8_t>(len);
        frame.push_back(static_cast<char>(b1));
    } else if (len <= 0xFFFF) {
        b1 |= 126;
        frame.push_back(static_cast<char>(b1));
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        b1 |= 127;
        frame.push_back(static_cast<char>(b1));
        uint64_t l64 = static_cast<uint64_t>(len);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>((l64 >> (i * 8)) & 0xFF));
        }
    }

    if (masked) {
        std::array<uint8_t, 4> mask{};
        if (custom_mask.has_value()) {
            mask = *custom_mask;
        } else {
            static std::atomic<uint32_t> seed{0x12345678};
            uint32_t s = seed.fetch_add(0x9e3779b9, std::memory_order_relaxed);
            mask = {
                static_cast<uint8_t>((s >> 24) & 0xFF),
                static_cast<uint8_t>((s >> 16) & 0xFF),
                static_cast<uint8_t>((s >> 8) & 0xFF),
                static_cast<uint8_t>(s & 0xFF)
            };
            if (mask[0] == 0 && mask[1] == 0 && mask[2] == 0 && mask[3] == 0) {
                mask[0] = 0x42;
            }
        }

        for (size_t i = 0; i < 4; ++i) {
            frame.push_back(static_cast<char>(mask[i]));
        }
        for (size_t i = 0; i < len; ++i) {
            frame.push_back(static_cast<char>(payload[i] ^ mask[i % 4]));
        }
    } else {
        frame.append(payload);
    }

    return frame;
}

struct WebSocketFrame {
    bool fin = true;
    bool rsv1 = false;
    bool rsv2 = false;
    bool rsv3 = false;
    WebSocketOpcode opcode = WebSocketOpcode::Text;
    bool masked = false;
    std::array<uint8_t, 4> mask_key{};
    std::string payload;
};

enum class FrameParseStatus {
    Ok,
    NeedMoreData,
    ProtocolError
};

struct FrameParseResult {
    FrameParseStatus status = FrameParseStatus::NeedMoreData;
    WebSocketFrame frame;
    size_t bytes_consumed = 0;
    uint16_t error_code = 1002;
    std::string error_message;
};

/// Parses a single RFC 6455 WebSocket frame from buffer.
inline FrameParseResult parse_websocket_frame(
    std::string_view buf,
    size_t max_allowed_size = 64 * 1024 * 1024
) {
    FrameParseResult result;
    if (buf.size() < 2) {
        result.status = FrameParseStatus::NeedMoreData;
        return result;
    }

    uint8_t b0 = static_cast<uint8_t>(buf[0]);
    bool fin = (b0 & 0x80) != 0;
    bool rsv1 = (b0 & 0x40) != 0;
    bool rsv2 = (b0 & 0x20) != 0;
    bool rsv3 = (b0 & 0x10) != 0;

    if (rsv1 || rsv2 || rsv3) {
        result.status = FrameParseStatus::ProtocolError;
        result.error_code = 1002;
        result.error_message = "RSV bits must be 0";
        return result;
    }

    uint8_t op_val = b0 & 0x0F;
    if (op_val != 0x0 && op_val != 0x1 && op_val != 0x2 &&
        op_val != 0x8 && op_val != 0x9 && op_val != 0xA) {
        result.status = FrameParseStatus::ProtocolError;
        result.error_code = 1002;
        result.error_message = "Unknown or unsupported opcode: " + std::to_string(op_val);
        return result;
    }

    auto opcode = static_cast<WebSocketOpcode>(op_val);
    bool is_control = (op_val >= 0x8);

    if (is_control && !fin) {
        result.status = FrameParseStatus::ProtocolError;
        result.error_code = 1002;
        result.error_message = "Control frames must not be fragmented";
        return result;
    }

    uint8_t b1 = static_cast<uint8_t>(buf[1]);
    bool masked = (b1 & 0x80) != 0;
    uint8_t len7 = b1 & 0x7F;

    if (is_control && len7 > 125) {
        result.status = FrameParseStatus::ProtocolError;
        result.error_code = 1002;
        result.error_message = "Control frame payload exceeds 125 bytes";
        return result;
    }

    size_t header_len = 2;
    uint64_t payload_len = 0;

    if (len7 <= 125) {
        payload_len = len7;
    } else if (len7 == 126) {
        header_len += 2;
        if (buf.size() < header_len) {
            result.status = FrameParseStatus::NeedMoreData;
            return result;
        }
        payload_len = (static_cast<uint64_t>(static_cast<uint8_t>(buf[2])) << 8) |
                      static_cast<uint64_t>(static_cast<uint8_t>(buf[3]));
        if (payload_len < 126) {
            result.status = FrameParseStatus::ProtocolError;
            result.error_code = 1002;
            result.error_message = "Non-minimal extended length 126 used for length < 126";
            return result;
        }
    } else { // len7 == 127
        header_len += 8;
        if (buf.size() < header_len) {
            result.status = FrameParseStatus::NeedMoreData;
            return result;
        }
        if ((static_cast<uint8_t>(buf[2]) & 0x80) != 0) {
            result.status = FrameParseStatus::ProtocolError;
            result.error_code = 1002;
            result.error_message = "Most significant bit of 64-bit length must be 0";
            return result;
        }
        payload_len = 0;
        for (size_t i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | static_cast<uint64_t>(static_cast<uint8_t>(buf[2 + i]));
        }
        if (payload_len <= 0xFFFF) {
            result.status = FrameParseStatus::ProtocolError;
            result.error_code = 1002;
            result.error_message = "Non-minimal extended length 127 used for length <= 65535";
            return result;
        }
    }

    if (payload_len > max_allowed_size) {
        result.status = FrameParseStatus::ProtocolError;
        result.error_code = 1009; // Message Too Big
        result.error_message = "Frame payload length exceeds maximum allowed limit";
        return result;
    }

    std::array<uint8_t, 4> mask_key{};
    if (masked) {
        if (buf.size() < header_len + 4) {
            result.status = FrameParseStatus::NeedMoreData;
            return result;
        }
        for (size_t i = 0; i < 4; ++i) {
            mask_key[i] = static_cast<uint8_t>(buf[header_len + i]);
        }
        header_len += 4;
    }

    if (buf.size() < header_len + payload_len) {
        result.status = FrameParseStatus::NeedMoreData;
        return result;
    }

    result.frame.fin = fin;
    result.frame.rsv1 = rsv1;
    result.frame.rsv2 = rsv2;
    result.frame.rsv3 = rsv3;
    result.frame.opcode = opcode;
    result.frame.masked = masked;
    result.frame.mask_key = mask_key;

    result.frame.payload.resize(payload_len);
    const char* src = buf.data() + header_len;
    if (masked) {
        for (size_t i = 0; i < payload_len; ++i) {
            result.frame.payload[i] = src[i] ^ mask_key[i % 4];
        }
    } else {
        result.frame.payload.assign(src, payload_len);
    }

    result.bytes_consumed = header_len + payload_len;
    result.status = FrameParseStatus::Ok;
    return result;
}

class WebSocket : public std::enable_shared_from_this<WebSocket> {
public:
    net::detail::socket_t socket_fd = net::detail::kInvalidSocket;
    std::atomic<WebSocketState> state{WebSocketState::Connecting};
    std::string path;
    std::string protocol;
    std::string pending_buffer;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query;
    HeaderMap headers;

    int write_timeout_ms = 10000;
    size_t max_message_size = 64 * 1024 * 1024;
    bool require_client_mask = true;

    // Callbacks
    std::function<void(const std::string&)> on_message_;
    std::function<void(const std::string&)> on_binary_;
    std::function<void(uint16_t, const std::string&)> on_close_;
    std::function<void(const std::string&)> on_ping_;
    std::function<void(const std::string&)> on_pong_;
    std::function<void(const std::string&)> on_error_;

    WebSocket() = default;
    WebSocket(net::detail::socket_t fd,
              std::string req_path,
              std::string subproto = "",
              std::unordered_map<std::string, std::string> route_params = {},
              std::unordered_map<std::string, std::string> query_params = {},
              HeaderMap hdrs = {})
        : socket_fd(fd),
          state(WebSocketState::Open),
          path(std::move(req_path)),
          protocol(std::move(subproto)),
          params(std::move(route_params)),
          query(std::move(query_params)),
          headers(std::move(hdrs)) {}

    ~WebSocket() {
        close(1001, "Going Away");
    }

    WebSocket(const WebSocket&) = delete;
    WebSocket& operator=(const WebSocket&) = delete;
    WebSocket(WebSocket&&) = delete;
    WebSocket& operator=(WebSocket&&) = delete;

    [[nodiscard]] bool is_open() const noexcept {
        return state.load() == WebSocketState::Open && socket_fd != net::detail::kInvalidSocket;
    }

    [[nodiscard]] net::detail::socket_t socket() const noexcept {
        return socket_fd;
    }

    [[nodiscard]] std::string param(std::string_view name, std::string_view fallback = "") const {
        auto it = params.find(std::string(name));
        return (it != params.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string query_param(std::string_view name, std::string_view fallback = "") const {
        auto it = query.find(std::string(name));
        return (it != query.end()) ? it->second : std::string(fallback);
    }

    [[nodiscard]] std::string header(std::string_view name, std::string_view fallback = "") const {
        return headers.get_or(name, fallback);
    }

    void set_require_mask(bool req) noexcept {
        require_client_mask = req;
    }

    WebSocket& onMessage(std::function<void(const std::string&)> cb) {
        on_message_ = std::move(cb);
        return *this;
    }

    WebSocket& onBinary(std::function<void(const std::string&)> cb) {
        on_binary_ = std::move(cb);
        return *this;
    }

    WebSocket& onClose(std::function<void(uint16_t, const std::string&)> cb) {
        on_close_ = std::move(cb);
        return *this;
    }

    WebSocket& onPing(std::function<void(const std::string&)> cb) {
        on_ping_ = std::move(cb);
        return *this;
    }

    WebSocket& onPong(std::function<void(const std::string&)> cb) {
        on_pong_ = std::move(cb);
        return *this;
    }

    WebSocket& onError(std::function<void(const std::string&)> cb) {
        on_error_ = std::move(cb);
        return *this;
    }

    bool send(std::string_view text) {
        return send_frame(WebSocketOpcode::Text, text);
    }

    bool sendBinary(std::string_view bytes) {
        return send_frame(WebSocketOpcode::Binary, bytes);
    }

    bool ping(std::string_view payload = "") {
        return send_frame(WebSocketOpcode::Ping, payload);
    }

    bool pong(std::string_view payload = "") {
        return send_frame(WebSocketOpcode::Pong, payload);
    }

    bool send_frame(WebSocketOpcode opcode, std::string_view payload, bool fin = true) {
        if (state.load() != WebSocketState::Open) {
            return false;
        }
        std::string frame = serialize_websocket_frame(opcode, payload, /*masked=*/false, std::nullopt, fin);
        std::lock_guard<std::recursive_mutex> lock(send_mutex_);
        if (socket_fd == net::detail::kInvalidSocket) {
            return false;
        }
        return detail::socket_send_all(socket_fd, frame.data(), frame.size(), write_timeout_ms);
    }

    bool send_frame_direct(WebSocketOpcode opcode, std::string_view payload, bool fin = true) {
        std::string frame = serialize_websocket_frame(opcode, payload, /*masked=*/false, std::nullopt, fin);
        std::lock_guard<std::recursive_mutex> lock(send_mutex_);
        if (socket_fd == net::detail::kInvalidSocket) {
            return false;
        }
        return detail::socket_send_all(socket_fd, frame.data(), frame.size(), write_timeout_ms);
    }

    void close(uint16_t code = 1000, std::string_view reason = "") {
        bool should_send = false;
        {
            std::lock_guard<std::recursive_mutex> lock(send_mutex_);
            if (state.load() == WebSocketState::Closed) {
                return;
            }
            if (state.load() == WebSocketState::Open) {
                state.store(WebSocketState::Closing);
                should_send = true;
            }
        }

        if (should_send) {
            std::string payload;
            payload.reserve(2 + reason.size());
            payload.push_back(static_cast<char>((code >> 8) & 0xFF));
            payload.push_back(static_cast<char>(code & 0xFF));
            payload.append(reason);

            std::string frame = serialize_websocket_frame(WebSocketOpcode::Close, payload, /*masked=*/false);
            std::lock_guard<std::recursive_mutex> lock(send_mutex_);
            if (socket_fd != net::detail::kInvalidSocket) {
                (void)detail::socket_send_all(socket_fd, frame.data(), frame.size(), 2000);
            }
        }

        close_socket();
        state.store(WebSocketState::Closed);
        trigger_close_callback(code, std::string(reason));
    }

    void run_loop(int poll_timeout_ms = 1000) {
        std::string in_buf = std::move(pending_buffer);
        std::array<char, 8192> recv_buf;

        while (state.load() == WebSocketState::Open && socket_fd != net::detail::kInvalidSocket) {
            while (!in_buf.empty()) {
                auto res = parse_websocket_frame(in_buf, max_message_size);
                if (res.status == FrameParseStatus::Ok) {
                    in_buf.erase(0, res.bytes_consumed);
                    if (!handle_frame(res.frame)) {
                        return;
                    }
                    if (state.load() != WebSocketState::Open) {
                        return;
                    }
                } else if (res.status == FrameParseStatus::ProtocolError) {
                    if (on_error_) {
                        try { on_error_(res.error_message); } catch (...) {}
                    }
                    close(res.error_code, res.error_message);
                    return;
                } else {
                    // NeedMoreData
                    break;
                }
            }

            if (state.load() != WebSocketState::Open || socket_fd == net::detail::kInvalidSocket) {
                break;
            }

            int pr = detail::socket_poll(socket_fd, POLLIN, poll_timeout_ms);
            if (pr < 0) {
                break;
            }
            if (pr == 0) {
                continue;
            }

            ssize_t n = detail::socket_recv(socket_fd, recv_buf.data(), recv_buf.size());
            if (n <= 0) {
                break;
            }
            in_buf.append(recv_buf.data(), static_cast<size_t>(n));
        }

        if (state.load() == WebSocketState::Open) {
            state.store(WebSocketState::Closed);
            close_socket();
            trigger_close_callback(1006, "Connection closed abnormally");
        }
    }

private:
    bool handle_frame(const WebSocketFrame& frame) {
        if (require_client_mask && !frame.masked) {
            if (on_error_) {
                try { on_error_("Client frames must be masked"); } catch (...) {}
            }
            close(1002, "Client frames must be masked");
            return false;
        }

        if (frame.opcode == WebSocketOpcode::Ping) {
            send_frame_direct(WebSocketOpcode::Pong, frame.payload);
            if (on_ping_) {
                try { on_ping_(frame.payload); } catch (...) {}
            }
            return true;
        }

        if (frame.opcode == WebSocketOpcode::Pong) {
            if (on_pong_) {
                try { on_pong_(frame.payload); } catch (...) {}
            }
            return true;
        }

        if (frame.opcode == WebSocketOpcode::Close) {
            uint16_t code = 1000;
            std::string reason;

            if (frame.payload.size() == 1) {
                if (on_error_) {
                    try { on_error_("Close payload cannot be 1 byte"); } catch (...) {}
                }
                close(1002, "Close payload cannot be 1 byte");
                return false;
            }

            if (frame.payload.size() >= 2) {
                code = (static_cast<uint8_t>(frame.payload[0]) << 8) |
                       static_cast<uint8_t>(frame.payload[1]);
                reason = frame.payload.substr(2);
                if (!is_valid_utf8(reason)) {
                    if (on_error_) {
                        try { on_error_("Close reason is not valid UTF-8"); } catch (...) {}
                    }
                    close(1007, "Close reason is not valid UTF-8");
                    return false;
                }
            }

            if (state.load() == WebSocketState::Open) {
                state.store(WebSocketState::Closing);
                send_frame_direct(WebSocketOpcode::Close, frame.payload);
            }

            close_socket();
            state.store(WebSocketState::Closed);
            trigger_close_callback(code, reason);
            return false;
        }

        if (frame.opcode == WebSocketOpcode::Continuation) {
            if (!in_fragmented_) {
                if (on_error_) {
                    try { on_error_("Unexpected continuation frame"); } catch (...) {}
                }
                close(1002, "Unexpected continuation frame");
                return false;
            }

            if (fragment_buffer_.size() + frame.payload.size() > max_message_size) {
                if (on_error_) {
                    try { on_error_("Message size exceeded limit"); } catch (...) {}
                }
                close(1009, "Message size exceeded limit");
                return false;
            }

            fragment_buffer_ += frame.payload;

            if (frame.fin) {
                in_fragmented_ = false;
                auto op = fragment_opcode_;
                std::string complete_msg = std::move(fragment_buffer_);
                fragment_buffer_.clear();

                if (op == WebSocketOpcode::Text) {
                    if (!is_valid_utf8(complete_msg)) {
                        if (on_error_) {
                            try { on_error_("Text frame contains invalid UTF-8"); } catch (...) {}
                        }
                        close(1007, "Text frame contains invalid UTF-8");
                        return false;
                    }
                    if (on_message_) {
                        try { on_message_(complete_msg); } catch (...) {}
                    }
                } else if (op == WebSocketOpcode::Binary) {
                    if (on_binary_) {
                        try { on_binary_(complete_msg); } catch (...) {}
                    }
                }
            }
            return true;
        }

        if (frame.opcode == WebSocketOpcode::Text || frame.opcode == WebSocketOpcode::Binary) {
            if (in_fragmented_) {
                if (on_error_) {
                    try { on_error_("Received new data frame before continuation finished"); } catch (...) {}
                }
                close(1002, "Received new data frame before continuation finished");
                return false;
            }

            if (frame.payload.size() > max_message_size) {
                if (on_error_) {
                    try { on_error_("Message size exceeded limit"); } catch (...) {}
                }
                close(1009, "Message size exceeded limit");
                return false;
            }

            if (frame.fin) {
                if (frame.opcode == WebSocketOpcode::Text) {
                    if (!is_valid_utf8(frame.payload)) {
                        if (on_error_) {
                            try { on_error_("Text frame contains invalid UTF-8"); } catch (...) {}
                        }
                        close(1007, "Text frame contains invalid UTF-8");
                        return false;
                    }
                    if (on_message_) {
                        try { on_message_(frame.payload); } catch (...) {}
                    }
                } else {
                    if (on_binary_) {
                        try { on_binary_(frame.payload); } catch (...) {}
                    }
                }
            } else {
                in_fragmented_ = true;
                fragment_opcode_ = frame.opcode;
                fragment_buffer_ = frame.payload;
            }
            return true;
        }

        return true;
    }

    void close_socket() noexcept {
        std::lock_guard<std::recursive_mutex> lock(send_mutex_);
        if (socket_fd != net::detail::kInvalidSocket) {
            net::detail::close_raw_socket(socket_fd);
            socket_fd = net::detail::kInvalidSocket;
        }
    }

    void trigger_close_callback(uint16_t code, const std::string& reason) {
        bool expected = false;
        if (close_dispatched_.compare_exchange_strong(expected, true)) {
            if (on_close_) {
                try {
                    on_close_(code, reason);
                } catch (...) {}
            }
        }
    }

    std::recursive_mutex send_mutex_;
    std::atomic<bool> close_dispatched_{false};
    bool in_fragmented_ = false;
    WebSocketOpcode fragment_opcode_ = WebSocketOpcode::Text;
    std::string fragment_buffer_;
};

// ============================================================================
// Worker Thread Pool
// ============================================================================

class HttpThreadPool {
public:
    explicit HttpThreadPool(size_t thread_count = 4) {
        if (thread_count == 0) {
            thread_count = 1;
        }
        workers_.reserve(thread_count);
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this]() {
                worker_loop();
            });
        }
    }

    ~HttpThreadPool() {
        stop();
    }

    HttpThreadPool(const HttpThreadPool&) = delete;
    HttpThreadPool& operator=(const HttpThreadPool&) = delete;

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    [[nodiscard]] size_t size() const noexcept {
        return workers_.size();
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    return stopping_ || !tasks_.empty();
                });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            if (task) {
                try {
                    task();
                } catch (...) {
                    // Swallow exceptions to keep worker threads alive
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

// ============================================================================
// Phase 2.1: Radix Tree Router & Parameter Extraction
// ============================================================================

enum class RouteMatchStatus {
    Found,
    MethodNotAllowed,
    NotFound
};

template <typename HandlerType>
struct RouteMatchResult {
    RouteMatchStatus status = RouteMatchStatus::NotFound;
    HandlerType handler{};
    std::unordered_map<std::string, std::string> params;
    std::vector<std::string> allowed_methods;
};

template <typename HandlerType>
struct RadixNode {
    std::string segment;
    enum class SegmentType { Static, Param, Wildcard } type = SegmentType::Static;
    std::string param_name;

    std::unordered_map<std::string, HandlerType> handlers;
    std::optional<HandlerType> all_handler;

    std::vector<std::unique_ptr<RadixNode<HandlerType>>> static_children;
    std::unique_ptr<RadixNode<HandlerType>> param_child;
    std::unique_ptr<RadixNode<HandlerType>> wildcard_child;

    [[nodiscard]] bool has_any_handler() const noexcept {
        return all_handler.has_value() || !handlers.empty();
    }

    [[nodiscard]] std::vector<std::string> allowed_methods() const {
        std::vector<std::string> methods;
        methods.reserve(handlers.size());
        for (const auto& [method, _] : handlers) {
            methods.push_back(method);
        }
        std::sort(methods.begin(), methods.end());
        return methods;
    }

    std::unique_ptr<RadixNode<HandlerType>> clone() const {
        auto copy = std::make_unique<RadixNode<HandlerType>>();
        copy->segment = segment;
        copy->type = type;
        copy->param_name = param_name;
        copy->handlers = handlers;
        copy->all_handler = all_handler;
        if (param_child) {
            copy->param_child = param_child->clone();
        }
        if (wildcard_child) {
            copy->wildcard_child = wildcard_child->clone();
        }
        copy->static_children.reserve(static_children.size());
        for (const auto& child : static_children) {
            copy->static_children.push_back(child->clone());
        }
        return copy;
    }
};

template <typename HandlerType>
class RadixRouter {
public:
    RadixRouter() : root_(std::make_unique<RadixNode<HandlerType>>()) {}

    RadixRouter(const RadixRouter& other)
        : root_(other.root_ ? other.root_->clone() : std::make_unique<RadixNode<HandlerType>>()),
          route_count_(other.route_count_) {}

    RadixRouter& operator=(const RadixRouter& other) {
        if (this != &other) {
            root_ = other.root_ ? other.root_->clone() : std::make_unique<RadixNode<HandlerType>>();
            route_count_ = other.route_count_;
        }
        return *this;
    }

    RadixRouter(RadixRouter&&) noexcept = default;
    RadixRouter& operator=(RadixRouter&&) noexcept = default;

    void add(std::string_view method, std::string_view path, HandlerType handler) {
        std::string norm_method = detail::to_upper_ascii(method);
        std::vector<std::string> segs = detail::split_path(path);

        RadixNode<HandlerType>* current = root_.get();

        for (const auto& seg : segs) {
            if (!seg.empty() && seg.front() == ':') {
                std::string param_name = seg.substr(1);
                if (!current->param_child) {
                    current->param_child = std::make_unique<RadixNode<HandlerType>>();
                    current->param_child->segment = seg;
                    current->param_child->type = RadixNode<HandlerType>::SegmentType::Param;
                    current->param_child->param_name = param_name;
                }
                current = current->param_child.get();
            } else if (!seg.empty() && seg.front() == '*') {
                std::string param_name = (seg.size() > 1) ? seg.substr(1) : "*";
                if (!current->wildcard_child) {
                    current->wildcard_child = std::make_unique<RadixNode<HandlerType>>();
                    current->wildcard_child->segment = seg;
                    current->wildcard_child->type = RadixNode<HandlerType>::SegmentType::Wildcard;
                    current->wildcard_child->param_name = param_name;
                }
                current = current->wildcard_child.get();
                break;
            } else {
                RadixNode<HandlerType>* found = nullptr;
                for (auto& child : current->static_children) {
                    if (child->segment == seg) {
                        found = child.get();
                        break;
                    }
                }
                if (!found) {
                    auto new_child = std::make_unique<RadixNode<HandlerType>>();
                    new_child->segment = seg;
                    new_child->type = RadixNode<HandlerType>::SegmentType::Static;
                    current->static_children.push_back(std::move(new_child));
                    found = current->static_children.back().get();
                }
                current = found;
            }
        }

        if (norm_method == "ALL" || norm_method == "*") {
            current->all_handler = std::move(handler);
        } else {
            current->handlers[norm_method] = std::move(handler);
        }
        ++route_count_;
    }

    void all(std::string_view path, HandlerType handler) {
        add("ALL", path, std::move(handler));
    }

    void get(std::string_view path, HandlerType handler) {
        add("GET", path, std::move(handler));
    }

    void post(std::string_view path, HandlerType handler) {
        add("POST", path, std::move(handler));
    }

    void put(std::string_view path, HandlerType handler) {
        add("PUT", path, std::move(handler));
    }

    void del(std::string_view path, HandlerType handler) {
        add("DELETE", path, std::move(handler));
    }

    void patch(std::string_view path, HandlerType handler) {
        add("PATCH", path, std::move(handler));
    }

    void head(std::string_view path, HandlerType handler) {
        add("HEAD", path, std::move(handler));
    }

    void options(std::string_view path, HandlerType handler) {
        add("OPTIONS", path, std::move(handler));
    }

    RouteMatchResult<HandlerType> match(std::string_view method, std::string_view path) const {
        RouteMatchResult<HandlerType> result;
        std::string norm_method = detail::to_upper_ascii(method);
        std::vector<std::string> segs = detail::split_path(path);

        const RadixNode<HandlerType>* matched_node = nullptr;
        std::unordered_map<std::string, std::string> params;

        bool found_path = match_recursive(root_.get(), 0, segs, params, matched_node);

        if (!found_path && root_->wildcard_child && root_->wildcard_child->has_any_handler()) {
            std::string full_path = std::string(path);
            if (!full_path.empty() && full_path.front() == '/') {
                full_path.erase(0, 1);
            }
            params.clear();
            params[root_->wildcard_child->param_name] = full_path;
            matched_node = root_->wildcard_child.get();
            found_path = true;
        }

        if (!found_path || !matched_node) {
            result.status = RouteMatchStatus::NotFound;
            return result;
        }

        result.params = std::move(params);

        auto it = matched_node->handlers.find(norm_method);
        if (it != matched_node->handlers.end()) {
            result.status = RouteMatchStatus::Found;
            result.handler = it->second;
            return result;
        }

        if (matched_node->all_handler.has_value()) {
            result.status = RouteMatchStatus::Found;
            result.handler = *matched_node->all_handler;
            return result;
        }

        result.status = RouteMatchStatus::MethodNotAllowed;
        result.allowed_methods = matched_node->allowed_methods();
        return result;
    }

    [[nodiscard]] bool empty() const noexcept {
        return route_count_ == 0;
    }

    [[nodiscard]] size_t size() const noexcept {
        return route_count_;
    }

private:
    bool match_recursive(
        const RadixNode<HandlerType>* node,
        size_t seg_idx,
        const std::vector<std::string>& segs,
        std::unordered_map<std::string, std::string>& params,
        const RadixNode<HandlerType>*& matched_node
    ) const {
        if (seg_idx == segs.size()) {
            if (node->has_any_handler()) {
                matched_node = node;
                return true;
            }
            if (node->wildcard_child && node->wildcard_child->has_any_handler()) {
                params[node->wildcard_child->param_name] = "";
                matched_node = node->wildcard_child.get();
                return true;
            }
            return false;
        }

        const std::string& current_seg = segs[seg_idx];

        // 1. Static children (highest priority)
        for (const auto& child : node->static_children) {
            if (child->segment == current_seg) {
                if (match_recursive(child.get(), seg_idx + 1, segs, params, matched_node)) {
                    return true;
                }
            }
        }

        // 2. Named parameter child
        if (node->param_child) {
            params[node->param_child->param_name] = current_seg;
            if (match_recursive(node->param_child.get(), seg_idx + 1, segs, params, matched_node)) {
                return true;
            }
            params.erase(node->param_child->param_name);
        }

        // 3. Wildcard child
        if (node->wildcard_child) {
            std::string rest;
            for (size_t i = seg_idx; i < segs.size(); ++i) {
                if (!rest.empty()) {
                    rest += "/";
                }
                rest += segs[i];
            }
            params[node->wildcard_child->param_name] = rest;
            matched_node = node->wildcard_child.get();
            return true;
        }

        return false;
    }

    std::unique_ptr<RadixNode<HandlerType>> root_;
    size_t route_count_ = 0;
};

using HttpHandler = std::function<HttpResponseMessage(const HttpRequestMessage&)>;
using HttpRouter = RadixRouter<HttpHandler>;

// ============================================================================
// Server Configuration & Lifecycle Engine
// ============================================================================

struct HttpServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    size_t worker_threads = 4;
    int keep_alive_timeout_ms = 5000;
    size_t max_keep_alive_requests = 100;
    int read_timeout_ms = 10000;
    int write_timeout_ms = 10000;
    int drain_timeout_ms = 3000;
    bool reuse_port = false;
    HttpParserLimits limits{};
};

class HttpServer {
public:
    explicit HttpServer(HttpServerConfig config = {})
        : config_(std::move(config)),
          thread_pool_(config_.worker_threads) {}

    explicit HttpServer(bool reuse_port)
        : thread_pool_(config_.worker_threads) {
        config_.reuse_port = reuse_port;
    }

    HttpServer(HttpHandler handler, HttpServerConfig config = {})
        : config_(std::move(config)),
          thread_pool_(config_.worker_threads),
          handler_(std::move(handler)) {}

    HttpServer(HttpHandler handler, bool reuse_port)
        : thread_pool_(config_.worker_threads),
          handler_(std::move(handler)) {
        config_.reuse_port = reuse_port;
    }

    HttpServer(Handler handler, HttpServerConfig config = {})
        : config_(std::move(config)),
          thread_pool_(config_.worker_threads) {
        set_handler(std::move(handler));
    }

    HttpServer(Handler handler, bool reuse_port)
        : thread_pool_(config_.worker_threads) {
        config_.reuse_port = reuse_port;
        set_handler(std::move(handler));
    }

    ~HttpServer() {
        stop();
    }

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void set_handler(HttpHandler handler) {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        handler_ = std::move(handler);
    }

    void use(Middleware mw) {
        use("", std::move(mw));
    }

    void use(std::string_view prefix, Middleware mw) {
        std::lock_guard<std::mutex> lock(middleware_mutex_);
        MiddlewareEntry entry;
        entry.prefix = std::string(prefix);
        entry.is_error_handler = false;
        entry.handler = std::move(mw);
        middlewares_.push_back(std::move(entry));
    }

    void use(ErrorMiddleware emw) {
        use("", std::move(emw));
    }

    void use(std::string_view prefix, ErrorMiddleware emw) {
        std::lock_guard<std::mutex> lock(middleware_mutex_);
        MiddlewareEntry entry;
        entry.prefix = std::string(prefix);
        entry.is_error_handler = true;
        entry.error_handler = std::move(emw);
        middlewares_.push_back(std::move(entry));
    }

    void set_handler(Handler handler) {
        set_handler([h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void get(std::string_view path, Handler handler) {
        get(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void get(std::string_view path, Middleware middleware, Handler handler) {
        get(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void post(std::string_view path, Handler handler) {
        post(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void post(std::string_view path, Middleware middleware, Handler handler) {
        post(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void put(std::string_view path, Handler handler) {
        put(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void put(std::string_view path, Middleware middleware, Handler handler) {
        put(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void del(std::string_view path, Handler handler) {
        del(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void del(std::string_view path, Middleware middleware, Handler handler) {
        del(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void patch(std::string_view path, Handler handler) {
        patch(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void patch(std::string_view path, Middleware middleware, Handler handler) {
        patch(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void head(std::string_view path, Handler handler) {
        head(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void head(std::string_view path, Middleware middleware, Handler handler) {
        head(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void options(std::string_view path, Handler handler) {
        options(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void options(std::string_view path, Middleware middleware, Handler handler) {
        options(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void all(std::string_view path, Handler handler) {
        all(path, [h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            h(req, res);
            return res.to_message();
        });
    }

    void all(std::string_view path, Middleware middleware, Handler handler) {
        all(path, [mw = std::move(middleware), h = std::move(handler)](const HttpRequestMessage& raw_req) -> HttpResponseMessage {
            Request req(raw_req);
            Response res;
            bool next_called = false;
            std::optional<std::string> next_err;
            mw(req, res, NextFunction([&](std::optional<std::string> err) {
                next_called = true;
                next_err = std::move(err);
            }));
            if (next_called && !res.is_sent()) {
                if (next_err.has_value()) {
                    throw std::runtime_error(*next_err);
                }
                h(req, res);
            }
            return res.to_message();
        });
    }

    void get(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.get(path, std::move(handler));
    }

    void post(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.post(path, std::move(handler));
    }

    void put(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.put(path, std::move(handler));
    }

    void del(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.del(path, std::move(handler));
    }

    void patch(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.patch(path, std::move(handler));
    }

    void head(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.head(path, std::move(handler));
    }

    void options(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.options(path, std::move(handler));
    }

    void all(std::string_view path, HttpHandler handler) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_.all(path, std::move(handler));
    }

    void ws(std::string_view path, WebSocketHandler handler) {
        ws(path, std::vector<std::string>{}, std::move(handler));
    }

    void ws(std::string_view path, std::vector<std::string> subprotocols, WebSocketHandler handler) {
        std::lock_guard<std::mutex> lock(ws_router_mutex_);
        WebSocketRouteEntry entry;
        entry.subprotocols = std::move(subprotocols);
        entry.handler = std::move(handler);
        ws_router_.add("GET", path, std::move(entry));
    }

    void ws(std::string_view path, std::function<void(WebSocket&)> handler) {
        ws(path, std::vector<std::string>{}, [h = std::move(handler)](std::shared_ptr<WebSocket> ws) {
            if (ws) h(*ws);
        });
    }

    void ws(std::string_view path, std::vector<std::string> subprotocols, std::function<void(WebSocket&)> handler) {
        ws(path, std::move(subprotocols), [h = std::move(handler)](std::shared_ptr<WebSocket> ws) {
            if (ws) h(*ws);
        });
    }

    /// Serves a reactive KML / std.ui component tree at specified route with auto WebSocket bridging
    HttpServer& serveUi(std::string_view route, std::function<std::shared_ptr<void>()> factory, std::string title = "Keet App", std::string scopedCss = "");

    template <typename Factory, typename = std::enable_if_t<!std::is_convertible_v<Factory, std::function<std::shared_ptr<void>()>>>>
    HttpServer& serveUi(std::string_view route, Factory&& factory, std::string title = "Keet App", std::string scopedCss = "") {
        auto sharedFactory = [f = std::forward<Factory>(factory)]() -> std::shared_ptr<void> {
            using Ret = std::decay_t<decltype(f())>;
            if constexpr (std::is_pointer_v<Ret>) {
                return std::shared_ptr<void>(f());
            } else if constexpr (std::is_constructible_v<std::shared_ptr<void>, Ret>) {
                return f();
            } else {
                return std::make_shared<Ret>(f());
            }
        };
        return serveUi(route, std::function<std::shared_ptr<void>()>(std::move(sharedFactory)), std::move(title), std::move(scopedCss));
    }

    void use_router(const HttpRouter& r) {
        std::lock_guard<std::mutex> lock(router_mutex_);
        router_ = r;
    }

    [[nodiscard]] HttpRouter& router() noexcept {
        return router_;
    }

    [[nodiscard]] const HttpRouter& router() const noexcept {
        return router_;
    }

    /// Starts listening on the configured host/port in a background thread.
    /// Returns true on successful socket bind & listen, false otherwise.
    bool start() {
        if (running_.load()) {
            return false;
        }
        if (!bind_and_listen()) {
            return false;
        }
        running_.store(true);
        acceptor_thread_ = std::thread([this]() {
            acceptor_loop();
        });
        return true;
    }

    /// Enables or disables SO_REUSEPORT on the listener socket.
    HttpServer& reusePort(bool enabled = true) noexcept {
        config_.reuse_port = enabled;
        return *this;
    }

    HttpServer& setReusePort(bool enabled = true) noexcept {
        config_.reuse_port = enabled;
        return *this;
    }

    [[nodiscard]] bool isReusePort() const noexcept {
        return config_.reuse_port;
    }

    /// Convenience method: sets port/host and starts listening. Throws NetError on bind failure.
    HttpServer& listen(int port = -1, const std::string& host = "") {
        if (port >= 0) {
            config_.port = port;
        }
        if (!host.empty()) {
            config_.host = host;
        }
        start();
        return *this;
    }

    /// Starts listening and blocks current thread until server is stopped.
    bool listen_and_serve() {
        if (!start()) {
            return false;
        }
        if (acceptor_thread_.joinable()) {
            acceptor_thread_.join();
        }
        return true;
    }

    /// Gracefully stops accepting new connections and drains in-flight requests.
    void stop() {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false)) {
            return;
        }

        // Close listening socket to unblock accept/poll immediately
        if (listen_fd_ != net::detail::kInvalidSocket) {
            net::detail::socket_t fd = listen_fd_;
            listen_fd_ = net::detail::kInvalidSocket;
            net::detail::close_raw_socket(fd);
        }

        // Join acceptor loop thread
        if (acceptor_thread_.joinable()) {
            acceptor_thread_.join();
        }

        // Wait for active in-flight requests to drain
        {
            std::unique_lock<std::mutex> lock(in_flight_mutex_);
            in_flight_cv_.wait_for(lock, std::chrono::milliseconds(config_.drain_timeout_ms), [this]() {
                return in_flight_requests_.load() == 0;
            });
        }

        // Close active WebSocket sessions
        {
            std::lock_guard<std::mutex> lock(ws_sessions_mutex_);
            for (auto& weak_ws : active_ws_sessions_) {
                if (auto ws = weak_ws.lock()) {
                    ws->close(1001, "Server shutting down");
                }
            }
            active_ws_sessions_.clear();
        }

        // Stop worker pool
        thread_pool_.stop();
    }

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load();
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return is_running();
    }

    [[nodiscard]] int bound_port() const noexcept {
        return bound_port_;
    }

    [[nodiscard]] int port() const noexcept {
        return bound_port();
    }

    [[nodiscard]] const std::string& bound_host() const noexcept {
        return bound_host_;
    }

    [[nodiscard]] size_t active_connections() const noexcept {
        return active_connections_.load();
    }

    [[nodiscard]] size_t in_flight_requests() const noexcept {
        return in_flight_requests_.load();
    }

    [[nodiscard]] const HttpServerConfig& config() const noexcept {
        return config_;
    }

private:
    bool bind_and_listen() {
        net::detail::ensure_winsock_initialized();

        struct addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        std::string port_str = std::to_string(config_.port);
        const char* host_str = (config_.host.empty() || config_.host == "0.0.0.0") ? nullptr : config_.host.c_str();

        struct addrinfo* res = nullptr;
        int rc = ::getaddrinfo(host_str, port_str.c_str(), &hints, &res);
        if (rc != 0 || !res) {
            std::string err = (rc != 0) ? ::gai_strerror(rc) : "getaddrinfo returned empty list";
            throw keet::error(
                "NetError",
                "http.listen: failed to resolve bind address '" +
                (host_str ? std::string(host_str) : "0.0.0.0") + ":" + port_str + "' (" + err + ")"
            );
        }

        std::string last_err = "failed to bind socket";
        net::detail::socket_t s = net::detail::kInvalidSocket;
        for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
            s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (s == net::detail::kInvalidSocket) {
                last_err = net::detail::last_socket_error_str();
                continue;
            }

            detail::set_reuse_addr_port(s, config_.reuse_port);
            net::detail::configure_no_sigpipe(s);
            detail::set_tcp_nodelay(s, true);

            if (p->ai_family == AF_INET6) {
                int no = 0;
                ::setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&no), sizeof(no));
            }

            if (::bind(s, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen)) == 0) {
                listen_fd_ = s;
                break;
            }

            last_err = net::detail::last_socket_error_str();
            net::detail::close_raw_socket(s);
            s = net::detail::kInvalidSocket;
        }

        ::freeaddrinfo(res);

        if (listen_fd_ == net::detail::kInvalidSocket) {
            throw keet::error(
                "NetError",
                "http.listen: failed to bind on '" +
                (host_str ? std::string(host_str) : "0.0.0.0") + ":" + port_str + "' (" + last_err + ")"
            );
        }

        if (::listen(listen_fd_, 128) != 0) {
            std::string err = net::detail::last_socket_error_str();
            net::detail::close_raw_socket(listen_fd_);
            listen_fd_ = net::detail::kInvalidSocket;
            throw keet::error(
                "NetError",
                "http.listen: failed to listen on '" +
                (host_str ? std::string(host_str) : "0.0.0.0") + ":" + port_str + "' (" + err + ")"
            );
        }

        // Determine bound port
        struct sockaddr_storage ss{};
        socklen_t sslen = sizeof(ss);
        if (::getsockname(listen_fd_, reinterpret_cast<struct sockaddr*>(&ss), &sslen) == 0) {
            if (ss.ss_family == AF_INET) {
                bound_port_ = ntohs(reinterpret_cast<struct sockaddr_in*>(&ss)->sin_port);
            } else if (ss.ss_family == AF_INET6) {
                bound_port_ = ntohs(reinterpret_cast<struct sockaddr_in6*>(&ss)->sin6_port);
            }
        } else {
            bound_port_ = config_.port;
        }

        bound_host_ = config_.host.empty() ? "0.0.0.0" : config_.host;
        return true;
    }

    void acceptor_loop() {
        while (running_.load()) {
            int pr = detail::socket_poll(listen_fd_, POLLIN, 100);
            if (pr < 0) {
                if (!running_.load()) break;
                break;
            }
            if (pr == 0) {
                continue;
            }

            struct sockaddr_storage client_addr{};
            socklen_t client_len = sizeof(client_addr);
            net::detail::socket_t client_fd = ::accept(
                listen_fd_,
                reinterpret_cast<struct sockaddr*>(&client_addr),
                &client_len
            );
            if (client_fd == net::detail::kInvalidSocket) {
                if (!running_.load()) break;
                continue;
            }

            std::string client_ip = "127.0.0.1";
            int client_port = 0;
            if (client_addr.ss_family == AF_INET) {
                auto* s4 = reinterpret_cast<struct sockaddr_in*>(&client_addr);
                char buf[INET_ADDRSTRLEN];
                if (::inet_ntop(AF_INET, &(s4->sin_addr), buf, sizeof(buf))) {
                    client_ip = buf;
                }
                client_port = ntohs(s4->sin_port);
            } else if (client_addr.ss_family == AF_INET6) {
                auto* s6 = reinterpret_cast<struct sockaddr_in6*>(&client_addr);
                char buf[INET6_ADDRSTRLEN];
                if (::inet_ntop(AF_INET6, &(s6->sin6_addr), buf, sizeof(buf))) {
                    client_ip = buf;
                }
                client_port = ntohs(s6->sin6_port);
            }

            active_connections_.fetch_add(1);
            thread_pool_.enqueue([this, client_fd, client_ip, client_port]() {
                handle_connection(client_fd, client_ip, client_port);
                active_connections_.fetch_sub(1);
            });
        }
    }

    void handle_connection(net::detail::socket_t client_fd, std::string client_ip, int client_port) {
        net::detail::configure_no_sigpipe(client_fd);
        detail::set_tcp_nodelay(client_fd, true);

        HttpRequestParser parser(config_.limits);
        size_t requests_served = 0;
        std::array<char, 8192> read_buf;

        while (running_.load() && requests_served < config_.max_keep_alive_requests) {
            int timeout = (requests_served == 0) ? config_.read_timeout_ms : config_.keep_alive_timeout_ms;
            int pr = detail::socket_poll(client_fd, POLLIN, timeout);
            if (pr <= 0) {
                // Timeout or poll error
                break;
            }

            ssize_t n = detail::socket_recv(client_fd, read_buf.data(), read_buf.size());
            if (n <= 0) {
                // Connection closed or error
                break;
            }

            if (!parser.feed(std::string_view(read_buf.data(), static_cast<size_t>(n)))) {
                // Protocol error
                HttpResponseMessage err_resp;
                err_resp.status = parser.error_code();
                err_resp.body = parser.error_message();
                err_resp.headers.set("Connection", "close");
                std::string raw = HttpResponseSerializer::serialize(err_resp);
                detail::socket_send_all(client_fd, raw.data(), raw.size(), config_.write_timeout_ms);
                break;
            }

            while (parser.is_complete()) {
                requests_served++;
                HttpRequestMessage raw_req = parser.message();
                raw_req.client_ip = client_ip;
                raw_req.client_port = client_port;

                in_flight_requests_.fetch_add(1);

                HttpResponseMessage resp;
                bool keep_alive_flag = raw_req.keep_alive;
                try {
                    Request req(raw_req);

                    // Check WebSocket routes
                    RouteMatchResult<WebSocketRouteEntry> ws_match;
                    {
                        std::lock_guard<std::mutex> lock(ws_router_mutex_);
                        if (!ws_router_.empty()) {
                            ws_match = ws_router_.match(req.method, req.path);
                        }
                    }

                    if (ws_match.status == RouteMatchStatus::Found) {
                        in_flight_requests_.fetch_sub(1);
                        in_flight_cv_.notify_all();

                        auto val = validate_websocket_upgrade(req, ws_match.handler.subprotocols);

                        if (val.status == WebSocketUpgradeStatus::VersionMismatch) {
                            HttpResponseMessage err_resp;
                            err_resp.status = 426;
                            err_resp.headers.set("Sec-WebSocket-Version", "13");
                            err_resp.headers.set("Connection", "close");
                            err_resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                            err_resp.body = "Upgrade Required: WebSocket version 13 required";
                            std::string raw = HttpResponseSerializer::serialize(err_resp);
                            detail::socket_send_all(client_fd, raw.data(), raw.size(), config_.write_timeout_ms);
                            net::detail::close_raw_socket(client_fd);
                            return;
                        }

                        if (val.status == WebSocketUpgradeStatus::InvalidKey) {
                            HttpResponseMessage err_resp;
                            err_resp.status = 400;
                            err_resp.headers.set("Connection", "close");
                            err_resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                            err_resp.body = "Bad Request: " + val.error_message;
                            std::string raw = HttpResponseSerializer::serialize(err_resp);
                            detail::socket_send_all(client_fd, raw.data(), raw.size(), config_.write_timeout_ms);
                            net::detail::close_raw_socket(client_fd);
                            return;
                        }

                        if (val.status != WebSocketUpgradeStatus::Success) {
                            HttpResponseMessage err_resp;
                            err_resp.status = 400;
                            err_resp.headers.set("Connection", "close");
                            err_resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                            err_resp.body = "Bad Request: Expected WebSocket Upgrade";
                            std::string raw = HttpResponseSerializer::serialize(err_resp);
                            detail::socket_send_all(client_fd, raw.data(), raw.size(), config_.write_timeout_ms);
                            net::detail::close_raw_socket(client_fd);
                            return;
                        }

                        // Handshake Success: Send 101 Switching Protocols
                        std::string handshake_raw = "HTTP/1.1 101 Switching Protocols\r\n"
                                                    "Upgrade: websocket\r\n"
                                                    "Connection: Upgrade\r\n"
                                                    "Sec-WebSocket-Accept: " + val.accept_key + "\r\n";
                        if (!val.selected_protocol.empty()) {
                            handshake_raw += "Sec-WebSocket-Protocol: " + val.selected_protocol + "\r\n";
                        }
                        handshake_raw += "\r\n";

                        if (!detail::socket_send_all(client_fd, handshake_raw.data(), handshake_raw.size(), config_.write_timeout_ms)) {
                            net::detail::close_raw_socket(client_fd);
                            return;
                        }

                        // Transition socket to WebSocket frame mode
                        auto ws = std::make_shared<WebSocket>(
                            client_fd, req.path, val.selected_protocol, ws_match.params, req.query, req.headers
                        );
                        ws->pending_buffer = parser.take_buffer();
                        ws->write_timeout_ms = config_.write_timeout_ms;

                        {
                            std::lock_guard<std::mutex> lock(ws_sessions_mutex_);
                            active_ws_sessions_.push_back(ws);
                        }

                        try {
                            ws_match.handler.handler(ws);
                        } catch (...) {
                            ws->close(1011, "Internal Server Error");
                            return;
                        }

                        ws->run_loop();

                        {
                            std::lock_guard<std::mutex> lock(ws_sessions_mutex_);
                            active_ws_sessions_.erase(
                                std::remove_if(active_ws_sessions_.begin(), active_ws_sessions_.end(),
                                    [](const std::weak_ptr<WebSocket>& w) { return w.expired(); }),
                                active_ws_sessions_.end());
                        }

                        if (ws->is_open()) {
                            ws->close();
                        }
                        return; // Connection is now managed and finished as a WebSocket session
                    }

                    Response res;

                    std::vector<MiddlewareEntry> mws;
                    {
                        std::lock_guard<std::mutex> lock(middleware_mutex_);
                        mws = middlewares_;
                    }

                    std::string current_error;
                    size_t total_mws = mws.size();

                    std::function<void(size_t, std::optional<std::string>)> dispatch;
                    dispatch = [&](size_t idx, std::optional<std::string> err) {
                        if (res.is_sent()) {
                            return;
                        }
                        if (err.has_value()) {
                            current_error = *err;
                        }

                        if (!current_error.empty()) {
                            while (idx < total_mws) {
                                const auto& entry = mws[idx];
                                idx++;
                                if (!entry.is_error_handler) continue;
                                if (!entry.prefix.empty() && !detail::path_starts_with_prefix(req.path, entry.prefix)) continue;

                                try {
                                    entry.error_handler(current_error, req, res, NextFunction([&, idx](std::optional<std::string> next_err) {
                                        dispatch(idx, next_err);
                                    }));
                                } catch (const std::exception& ex) {
                                    current_error = ex.what();
                                    dispatch(idx, current_error);
                                } catch (...) {
                                    current_error = "Unknown error";
                                    dispatch(idx, current_error);
                                }
                                return;
                            }

                            if (!res.is_sent()) {
                                res.status(500);
                                res.setHeader("Content-Type", "text/plain; charset=utf-8");
                                res.send("Internal Server Error: " + current_error);
                            }
                            return;
                        }

                        while (idx < total_mws) {
                            const auto& entry = mws[idx];
                            idx++;
                            if (entry.is_error_handler) continue;
                            if (!entry.prefix.empty() && !detail::path_starts_with_prefix(req.path, entry.prefix)) continue;

                            try {
                                entry.handler(req, res, NextFunction([&, idx](std::optional<std::string> next_err) {
                                    dispatch(idx, next_err);
                                }));
                            } catch (const std::exception& ex) {
                                current_error = ex.what();
                                dispatch(idx, current_error);
                            } catch (...) {
                                current_error = "Unknown error";
                                dispatch(idx, current_error);
                            }
                            return;
                        }

                        // Reached end of pipeline -> Route Matching!
                        if (!res.is_sent()) {
                            bool handled = false;
                            RouteMatchResult<HttpHandler> match_res;
                            {
                                std::lock_guard<std::mutex> lock(router_mutex_);
                                if (!router_.empty()) {
                                    match_res = router_.match(req.method, req.path);
                                }
                            }

                            if (match_res.status == RouteMatchStatus::Found) {
                                HttpRequestMessage enriched_raw = req.to_raw_message();
                                enriched_raw.params = std::move(match_res.params);
                                try {
                                    resp = match_res.handler(enriched_raw);
                                    res.status_code = resp.status;
                                    for (const auto& h : resp.headers.entries()) {
                                        res.setHeader(h.first, h.second);
                                    }
                                    res.body = resp.body;
                                    res.end();
                                } catch (const std::exception& ex) {
                                    current_error = ex.what();
                                    dispatch(0, current_error);
                                } catch (...) {
                                    current_error = "Unknown error";
                                    dispatch(0, "Unknown error");
                                }
                                handled = true;
                            } else if (match_res.status == RouteMatchStatus::MethodNotAllowed) {
                                res.status(405);
                                std::string allow_hdr;
                                for (size_t i = 0; i < match_res.allowed_methods.size(); ++i) {
                                    if (i > 0) {
                                        allow_hdr += ", ";
                                    }
                                    allow_hdr += match_res.allowed_methods[i];
                                }
                                res.setHeader("Allow", allow_hdr);
                                res.setHeader("Content-Type", "text/plain; charset=utf-8");
                                res.send("Method Not Allowed");
                                handled = true;
                            }

                            if (!handled) {
                                HttpHandler handler_copy;
                                {
                                    std::lock_guard<std::mutex> lock(handler_mutex_);
                                    handler_copy = handler_;
                                }
                                if (handler_copy) {
                                    try {
                                        resp = handler_copy(req.to_raw_message());
                                        res.status_code = resp.status;
                                        for (const auto& h : resp.headers.entries()) {
                                            res.setHeader(h.first, h.second);
                                        }
                                        res.body = resp.body;
                                        res.end();
                                    } catch (const std::exception& ex) {
                                        current_error = ex.what();
                                        dispatch(0, current_error);
                                    } catch (...) {
                                        current_error = "Unknown error";
                                        dispatch(0, "Unknown error");
                                    }
                                } else {
                                    res.status(404);
                                    res.setHeader("Content-Type", "text/plain; charset=utf-8");
                                    res.send("Cannot " + req.method + " " + req.path);
                                }
                            }
                        }
                    };

                    dispatch(0, std::nullopt);
                    resp = res.to_message();
                } catch (const std::exception& ex) {
                    resp.status = 500;
                    resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                    resp.body = std::string("Internal Server Error: ") + ex.what();
                } catch (...) {
                    resp.status = 500;
                    resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                    resp.body = "Internal Server Error";
                }

                in_flight_requests_.fetch_sub(1);
                in_flight_cv_.notify_all();

                bool will_keep_alive = (resp.status != 101) &&
                                       keep_alive_flag &&
                                       running_.load() &&
                                       (requests_served < config_.max_keep_alive_requests) &&
                                       (detail::to_lower_ascii(resp.headers.get_or("connection", "")) != "close");

                if (resp.status == 101) {
                    // Handshake response already sets Upgrade and Connection: Upgrade headers
                } else if (will_keep_alive) {
                    resp.headers.set("Connection", "keep-alive");
                    resp.headers.set("Keep-Alive", "timeout=" + std::to_string(config_.keep_alive_timeout_ms / 1000) +
                                                    ", max=" + std::to_string(config_.max_keep_alive_requests - requests_served));
                } else {
                    resp.headers.set("Connection", "close");
                }

                std::string raw = HttpResponseSerializer::serialize(resp);
                if (!detail::socket_send_all(client_fd, raw.data(), raw.size(), config_.write_timeout_ms)) {
                    net::detail::close_raw_socket(client_fd);
                    return;
                }

                if (resp.status == 101) {
                    return; // Switched protocols; socket handed over
                }

                if (!will_keep_alive) {
                    net::detail::close_raw_socket(client_fd);
                    return;
                }

                // Reset parser keeping leftover buffer bytes for pipelining
                parser.reset_keep_buffer();
                parser.feed("");
            }
        }

        net::detail::close_raw_socket(client_fd);
    }

    HttpServerConfig config_;
    HttpThreadPool thread_pool_;
    HttpHandler handler_;
    std::mutex handler_mutex_;

    HttpRouter router_;
    std::mutex router_mutex_;

    std::vector<MiddlewareEntry> middlewares_;
    std::mutex middleware_mutex_;

    RadixRouter<WebSocketRouteEntry> ws_router_;
    std::mutex ws_router_mutex_;

    std::vector<std::weak_ptr<WebSocket>> active_ws_sessions_;
    std::mutex ws_sessions_mutex_;

    net::detail::socket_t listen_fd_ = net::detail::kInvalidSocket;
    int bound_port_ = 0;
    std::string bound_host_;

    std::atomic<bool> running_{false};
    std::thread acceptor_thread_;

    std::atomic<size_t> active_connections_{0};
    std::atomic<size_t> in_flight_requests_{0};
    std::mutex in_flight_mutex_;
    std::condition_variable in_flight_cv_;
};

using App = HttpServer;
using Server = HttpServer;

/// Creates a new HTTP server instance with the given configuration.
inline HttpServer createServer(HttpServerConfig config = {}) {
    return HttpServer(std::move(config));
}

/// Creates a new HTTP server instance with the given reuse_port flag.
inline HttpServer createServer(bool reuse_port) {
    return HttpServer(reuse_port);
}

/// Creates a new HTTP server instance with options map.
inline HttpServer createServer(const std::unordered_map<std::string, bool>& opts) {
    HttpServerConfig cfg;
    auto it = opts.find("reusePort");
    if (it != opts.end()) {
        cfg.reuse_port = it->second;
    } else {
        auto it2 = opts.find("reuse_port");
        if (it2 != opts.end()) {
            cfg.reuse_port = it2->second;
        }
    }
    return HttpServer(std::move(cfg));
}

/// Creates a new HTTP server instance with the given default request handler.
inline HttpServer createServer(HttpHandler handler, HttpServerConfig config = {}) {
    return HttpServer(std::move(handler), std::move(config));
}

/// Creates a new HTTP server instance with the given default request handler and reuse_port flag.
inline HttpServer createServer(HttpHandler handler, bool reuse_port) {
    return HttpServer(std::move(handler), reuse_port);
}

/// Creates a new HTTP server instance with the given express-style request handler.
inline HttpServer createServer(Handler handler, HttpServerConfig config = {}) {
    return HttpServer(std::move(handler), std::move(config));
}

/// Creates a new HTTP server instance with the given express-style request handler and reuse_port flag.
inline HttpServer createServer(Handler handler, bool reuse_port) {
    return HttpServer(std::move(handler), reuse_port);
}

/// Creates a new Express-style HTTP server application.
inline HttpServer app(HttpServerConfig config = {}) {
    return HttpServer(std::move(config));
}

/// Creates a new Express-style HTTP server application with reuse_port flag.
inline HttpServer app(bool reuse_port) {
    return HttpServer(reuse_port);
}

/// Creates a new Express-style HTTP server application with options map.
inline HttpServer app(const std::unordered_map<std::string, bool>& opts) {
    HttpServerConfig cfg;
    auto it = opts.find("reusePort");
    if (it != opts.end()) {
        cfg.reuse_port = it->second;
    } else {
        auto it2 = opts.find("reuse_port");
        if (it2 != opts.end()) {
            cfg.reuse_port = it2->second;
        }
    }
    return HttpServer(std::move(cfg));
}

} // namespace keet::net::http

namespace keet {
namespace http = ::keet::net::http;
}
