#pragma once

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
#include <iomanip>
#include <mutex>
#include <optional>
#include <queue>
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
    std::string body;
    std::string client_ip = "127.0.0.1";
    int client_port = 0;
    bool keep_alive = true;
    bool upgrade_websocket = false;

    [[nodiscard]] std::string param(std::string_view name, std::string_view fallback = "") const {
        auto it = params.find(std::string(name));
        return (it != params.end()) ? it->second : std::string(fallback);
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
          locals(msg.locals) {
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

    [[nodiscard]] std::string cookie(std::string_view name, std::string_view fallback = "") const {
        auto it = cookies.find(std::string(name));
        return (it != cookies.end()) ? it->second : std::string(fallback);
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

inline void set_reuse_addr_port(net::detail::socket_t fd) noexcept {
    int one = 1;
#if defined(_WIN32)
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));
#else
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, static_cast<socklen_t>(sizeof(one)));
#if defined(SO_REUSEPORT)
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, static_cast<socklen_t>(sizeof(one)));
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
    HttpParserLimits limits{};
};

class HttpServer {
public:
    explicit HttpServer(HttpServerConfig config = {})
        : config_(std::move(config)),
          thread_pool_(config_.worker_threads) {}

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

    /// Convenience method: sets handler and starts listening.
    bool listen(int port = -1, const std::string& host = "") {
        if (port >= 0) {
            config_.port = port;
        }
        if (!host.empty()) {
            config_.host = host;
        }
        return start();
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

        // Stop worker pool
        thread_pool_.stop();
    }

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load();
    }

    [[nodiscard]] int bound_port() const noexcept {
        return bound_port_;
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
            return false;
        }

        net::detail::socket_t s = net::detail::kInvalidSocket;
        for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
            s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (s == net::detail::kInvalidSocket) {
                continue;
            }

            detail::set_reuse_addr_port(s);
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

            net::detail::close_raw_socket(s);
            s = net::detail::kInvalidSocket;
        }

        ::freeaddrinfo(res);

        if (listen_fd_ == net::detail::kInvalidSocket) {
            return false;
        }

        if (::listen(listen_fd_, 128) != 0) {
            net::detail::close_raw_socket(listen_fd_);
            listen_fd_ = net::detail::kInvalidSocket;
            return false;
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
                    Response res;

                    std::vector<MiddlewareEntry> mws;
                    {
                        std::lock_guard<std::mutex> lock(middleware_mutex_);
                        mws = middlewares_;
                    }

                    std::string current_error;
                    size_t mw_index = 0;

                    auto run_error_middlewares = [&](size_t start_idx) {
                        for (size_t i = start_idx; i < mws.size(); ++i) {
                            if (res.is_sent()) break;
                            const auto& entry = mws[i];
                            if (!entry.is_error_handler) continue;
                            if (!entry.prefix.empty() && !detail::path_starts_with_prefix(req.path, entry.prefix)) continue;

                            bool next_called = false;
                            std::optional<std::string> next_err;
                            try {
                                entry.error_handler(current_error, req, res, NextFunction([&](std::optional<std::string> err) {
                                    next_called = true;
                                    next_err = std::move(err);
                                }));
                            } catch (const std::exception& ex) {
                                current_error = ex.what();
                                continue;
                            } catch (...) {
                                current_error = "Unknown error";
                                continue;
                            }

                            if (res.is_sent()) break;
                            if (next_called) {
                                if (next_err.has_value()) {
                                    current_error = *next_err;
                                } else {
                                    current_error.clear();
                                    break;
                                }
                            } else {
                                break;
                            }
                        }
                    };

                    while (mw_index < mws.size() && !res.is_sent() && current_error.empty()) {
                        const auto& entry = mws[mw_index];
                        if (entry.is_error_handler) {
                            mw_index++;
                            continue;
                        }
                        if (!entry.prefix.empty() && !detail::path_starts_with_prefix(req.path, entry.prefix)) {
                            mw_index++;
                            continue;
                        }

                        bool next_called = false;
                        std::optional<std::string> next_err;
                        try {
                            entry.handler(req, res, NextFunction([&](std::optional<std::string> err) {
                                next_called = true;
                                next_err = std::move(err);
                            }));
                        } catch (const std::exception& ex) {
                            current_error = ex.what();
                            mw_index++;
                            break;
                        } catch (...) {
                            current_error = "Unknown error";
                            mw_index++;
                            break;
                        }

                        if (res.is_sent()) break;

                        if (next_called) {
                            if (next_err.has_value()) {
                                current_error = *next_err;
                                mw_index++;
                                break;
                            }
                            mw_index++;
                        } else {
                            break;
                        }
                    }

                    if (!current_error.empty() && !res.is_sent()) {
                        run_error_middlewares(mw_index);
                    }

                    if (res.is_sent()) {
                        resp = res.to_message();
                        keep_alive_flag = req.keep_alive;
                    } else if (!current_error.empty()) {
                        resp.status = 500;
                        resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                        resp.body = "Internal Server Error: " + current_error;
                    } else {
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
                            } catch (const std::exception& ex) {
                                current_error = ex.what();
                            } catch (...) {
                                current_error = "Unknown error";
                            }

                            if (!current_error.empty()) {
                                run_error_middlewares(0);
                                if (res.is_sent()) {
                                    resp = res.to_message();
                                } else {
                                    resp.status = 500;
                                    resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                                    resp.body = "Internal Server Error: " + current_error;
                                }
                            }
                            handled = true;
                        } else if (match_res.status == RouteMatchStatus::MethodNotAllowed) {
                            resp.status = 405;
                            std::string allow_hdr;
                            for (size_t i = 0; i < match_res.allowed_methods.size(); ++i) {
                                if (i > 0) {
                                    allow_hdr += ", ";
                                }
                                allow_hdr += match_res.allowed_methods[i];
                            }
                            resp.headers.set("Allow", allow_hdr);
                            resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                            resp.body = "Method Not Allowed";
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
                                } catch (const std::exception& ex) {
                                    current_error = ex.what();
                                } catch (...) {
                                    current_error = "Unknown error";
                                }

                                if (!current_error.empty()) {
                                    run_error_middlewares(0);
                                    if (res.is_sent()) {
                                        resp = res.to_message();
                                    } else {
                                        resp.status = 500;
                                        resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                                        resp.body = "Internal Server Error: " + current_error;
                                    }
                                }
                            } else {
                                resp.status = 404;
                                resp.headers.set("Content-Type", "text/plain; charset=utf-8");
                                resp.body = "Cannot " + req.method + " " + req.path;
                            }
                        }
                    }
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

                bool will_keep_alive = keep_alive_flag &&
                                       running_.load() &&
                                       (requests_served < config_.max_keep_alive_requests) &&
                                       (detail::to_lower_ascii(resp.headers.get_or("connection", "")) != "close");

                if (will_keep_alive) {
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

} // namespace keet::net::http
