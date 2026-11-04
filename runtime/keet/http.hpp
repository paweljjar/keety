#pragma once

#include <keet/error.hpp>
#include <keet/json.hpp>
#include <keet/net.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

    void set(std::string name, std::string value) {
        std::string key_lower = detail::to_lower_ascii(name);
        for (auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                entry.second = std::move(value);
                return;
            }
        }
        entries_.emplace_back(std::move(name), std::move(value));
    }

    void add(std::string name, std::string value) {
        std::string key_lower = detail::to_lower_ascii(name);
        for (auto& entry : entries_) {
            if (detail::to_lower_ascii(entry.first) == key_lower) {
                if (key_lower == "set-cookie") {
                    // Set-Cookie headers must be separate entries
                    entries_.emplace_back(std::move(name), std::move(value));
                } else {
                    entry.second += ", " + value;
                }
                return;
            }
        }
        entries_.emplace_back(std::move(name), std::move(value));
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
    std::string body;
    bool keep_alive = true;
    bool upgrade_websocket = false;
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

    void reset() {
        buffer_.clear();
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

} // namespace keet::net::http
