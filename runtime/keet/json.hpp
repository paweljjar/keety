#pragma once

#include <keet/bool_byte.hpp>
#include <keet/deep_copy_ptr.hpp>
#include <keet/error.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace keet::json {

namespace detail {

/// Internal RFC 8259 JSON DOM node representation
struct JsonNode {
    using Array = std::vector<JsonNode>;
    using Object = std::map<std::string, JsonNode>;
    using Value = std::variant<
        std::monostate, // null
        bool,           // boolean
        int64_t,        // exact 64-bit integer
        double,         // floating-point number
        std::string,    // unescaped UTF-8 string
        Array,          // JSON array
        Object          // JSON object (lexicographically ordered keys)
    >;

    Value value{std::monostate{}};

    JsonNode() = default;
    explicit JsonNode(std::monostate v) : value(v) {}
    explicit JsonNode(bool v) : value(v) {}
    explicit JsonNode(int64_t v) : value(v) {}
    explicit JsonNode(double v) : value(v) {}
    explicit JsonNode(std::string v) : value(std::move(v)) {}
    explicit JsonNode(Array v) : value(std::move(v)) {}
    explicit JsonNode(Object v) : value(std::move(v)) {}

    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::monostate>(value); }
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(value); }
    [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<int64_t>(value); }
    [[nodiscard]] bool is_float() const noexcept { return std::holds_alternative<double>(value); }
    [[nodiscard]] bool is_number() const noexcept { return is_int() || is_float(); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(value); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<Array>(value); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<Object>(value); }

    [[nodiscard]] std::string_view type_name() const noexcept {
        if (is_object()) return "object";
        if (is_array()) return "array";
        if (is_string()) return "string";
        if (is_number()) return "number";
        if (is_bool()) return "bool";
        return "null";
    }
};

/// Appends a Unicode code point (U+0000..U+10FFFF) as UTF-8 bytes into out.
inline void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | ((cp >> 6u) & 0x1Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | ((cp >> 12u) & 0x0Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | ((cp >> 18u) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        throw keet::error("JsonError", "Invalid Unicode code point out of range");
    }
}

/// Escapes raw UTF-8 string into a JSON double-quoted string literal.
[[nodiscard]] inline std::string escape_json_string(std::string_view str) {
    std::string out;
    out.reserve(str.size() + 2);
    out.push_back('"');
    for (unsigned char c : str) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20u) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

/// Formats a finite double into a clean JSON number representation.
[[nodiscard]] inline std::string format_double(double val, const char* context = "json") {
    if (!std::isfinite(val)) {
        throw keet::error("JsonError", std::string(context) + ": cannot serialize non-finite float (NaN or Infinity) to JSON");
    }
    if (val == 0.0) {
        return "0.0";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.15g", val);
    std::string res(buf);
    // Replace locale comma if any
    std::replace(res.begin(), res.end(), ',', '.');
    return res;
}

/// Recursive-descent RFC 8259 JSON parser with line/column diagnostics
class Parser {
public:
    static constexpr size_t MAX_DEPTH = 512;

    explicit Parser(std::string_view src) noexcept : src_(src) {}

    [[nodiscard]] JsonNode parse_document() {
        skip_whitespace();
        if (eof()) {
            fail("unexpected end of JSON input");
        }
        JsonNode root = parse_value(0);
        skip_whitespace();
        if (!eof()) {
            fail(std::string("unexpected trailing token '") + peek() + "' after JSON value");
        }
        return root;
    }

    [[nodiscard]] std::string parse_string_literal_only() {
        skip_whitespace();
        if (eof() || peek() != '"') {
            fail("expected '\"' at start of JSON string");
        }
        std::string res = parse_string();
        skip_whitespace();
        if (!eof()) {
            fail("unexpected trailing characters after JSON string");
        }
        return res;
    }

private:
    std::string_view src_;
    size_t pos_{0};
    size_t line_{1};
    size_t col_{1};

    [[nodiscard]] bool eof() const noexcept { return pos_ >= src_.size(); }

    [[nodiscard]] char peek() const noexcept { return pos_ < src_.size() ? src_[pos_] : '\0'; }

    char advance() noexcept {
        char c = src_[pos_++];
        if (c == '\n') {
            ++line_;
            col_ = 1;
        } else {
            ++col_;
        }
        return c;
    }

    [[noreturn]] void fail(const std::string& msg) const {
        throw keet::error(
            "JsonError",
            msg + " (at line " + std::to_string(line_) + ", column " + std::to_string(col_) + ")"
        );
    }

    void skip_whitespace() noexcept {
        while (!eof()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else {
                break;
            }
        }
    }

    [[nodiscard]] JsonNode parse_value(size_t depth) {
        if (depth > MAX_DEPTH) {
            fail("maximum JSON nesting depth exceeded");
        }
        skip_whitespace();
        if (eof()) {
            fail("unexpected end of JSON input while parsing value");
        }
        char c = peek();
        switch (c) {
        case '{': return parse_object(depth + 1);
        case '[': return parse_array(depth + 1);
        case '"': return JsonNode(parse_string());
        case 't': return parse_true();
        case 'f': return parse_false();
        case 'n': return parse_null();
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return parse_number();
            }
            fail(std::string("unexpected character '") + c + "'");
        }
    }

    [[nodiscard]] JsonNode parse_null() {
        if (src_.substr(pos_, 4) != "null") {
            fail("invalid literal, expected 'null'");
        }
        for (int i = 0; i < 4; ++i) advance();
        return JsonNode(std::monostate{});
    }

    [[nodiscard]] JsonNode parse_true() {
        if (src_.substr(pos_, 4) != "true") {
            fail("invalid literal, expected 'true'");
        }
        for (int i = 0; i < 4; ++i) advance();
        return JsonNode(true);
    }

    [[nodiscard]] JsonNode parse_false() {
        if (src_.substr(pos_, 5) != "false") {
            fail("invalid literal, expected 'false'");
        }
        for (int i = 0; i < 5; ++i) advance();
        return JsonNode(false);
    }

    [[nodiscard]] uint16_t parse_hex4() {
        if (pos_ + 4 > src_.size()) {
            fail("incomplete \\uXXXX hex escape in string");
        }
        uint16_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = advance();
            val <<= 4u;
            if (c >= '0' && c <= '9') {
                val |= static_cast<uint16_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                val |= static_cast<uint16_t>(10 + (c - 'a'));
            } else if (c >= 'A' && c <= 'F') {
                val |= static_cast<uint16_t>(10 + (c - 'A'));
            } else {
                fail(std::string("invalid hex digit '") + c + "' in \\uXXXX escape");
            }
        }
        return val;
    }

    [[nodiscard]] std::string parse_string() {
        if (advance() != '"') {
            fail("expected '\"' at start of string");
        }
        std::string out;
        while (!eof()) {
            char c = advance();
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20u) {
                fail("unescaped control character in JSON string");
            }
            if (c == '\\') {
                if (eof()) {
                    fail("unterminated escape sequence in JSON string");
                }
                char esc = advance();
                switch (esc) {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    uint16_t code_unit = parse_hex4();
                    if (code_unit >= 0xD800u && code_unit <= 0xDBFFu) {
                        // High surrogate: must be followed by \uXXXX low surrogate
                        if (pos_ + 2 > src_.size() || src_[pos_] != '\\' || src_[pos_ + 1] != 'u') {
                            fail("unpaired UTF-16 high surrogate in JSON string");
                        }
                        advance(); // '\'
                        advance(); // 'u'
                        uint16_t low_unit = parse_hex4();
                        if (low_unit < 0xDC00u || low_unit > 0xDFFFu) {
                            fail("invalid UTF-16 low surrogate following high surrogate");
                        }
                        uint32_t cp = 0x10000u + ((static_cast<uint32_t>(code_unit - 0xD800u) << 10u) |
                                                  static_cast<uint32_t>(low_unit - 0xDC00u));
                        append_utf8(out, cp);
                    } else if (code_unit >= 0xDC00u && code_unit <= 0xDFFFu) {
                        fail("unpaired UTF-16 low surrogate in JSON string");
                    } else {
                        append_utf8(out, static_cast<uint32_t>(code_unit));
                    }
                    break;
                }
                default:
                    fail(std::string("invalid escape sequence '\\") + esc + "' in JSON string");
                }
            } else {
                out.push_back(c);
            }
        }
        fail("unterminated JSON string literal");
    }

    [[nodiscard]] JsonNode parse_number() {
        size_t start = pos_;
        bool is_float = false;

        if (peek() == '-') {
            advance();
        }
        if (eof()) {
            fail("incomplete number literal");
        }
        if (peek() == '0') {
            advance();
            if (!eof() && peek() >= '0' && peek() <= '9') {
                fail("leading zeros are not permitted in JSON numbers");
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (!eof() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        } else {
            fail("invalid number literal");
        }

        if (!eof() && peek() == '.') {
            is_float = true;
            advance();
            if (eof() || peek() < '0' || peek() > '9') {
                fail("expected digit after decimal point in JSON number");
            }
            while (!eof() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }

        if (!eof() && (peek() == 'e' || peek() == 'E')) {
            is_float = true;
            advance();
            if (!eof() && (peek() == '+' || peek() == '-')) {
                advance();
            }
            if (eof() || peek() < '0' || peek() > '9') {
                fail("expected digit in exponent of JSON number");
            }
            while (!eof() && peek() >= '0' && peek() <= '9') {
                advance();
            }
        }

        std::string_view token = src_.substr(start, pos_ - start);
        if (!is_float) {
            int64_t ival = 0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), ival);
            if (ec == std::errc{} && ptr == token.data() + token.size()) {
                return JsonNode(ival);
            }
            // If integer overflows int64_t, fall back to double if representable
        }

        std::string num_str(token);
        char* end_ptr = nullptr;
        double dval = std::strtod(num_str.c_str(), &end_ptr);
        if (end_ptr != num_str.c_str() + num_str.size() || !std::isfinite(dval)) {
            fail("numeric literal out of representable range: '" + num_str + "'");
        }
        return JsonNode(dval);
    }

    [[nodiscard]] JsonNode parse_array(size_t depth) {
        advance(); // '['
        skip_whitespace();
        JsonNode::Array arr;
        if (!eof() && peek() == ']') {
            advance();
            return JsonNode(std::move(arr));
        }
        while (true) {
            arr.push_back(parse_value(depth));
            skip_whitespace();
            if (eof()) {
                fail("unterminated JSON array, expected ',' or ']'");
            }
            char c = peek();
            if (c == ']') {
                advance();
                break;
            }
            if (c != ',') {
                fail(std::string("expected ',' or ']' in JSON array, found '") + c + "'");
            }
            advance(); // ','
            skip_whitespace();
            if (!eof() && peek() == ']') {
                fail("trailing comma is not permitted in JSON array");
            }
        }
        return JsonNode(std::move(arr));
    }

    [[nodiscard]] JsonNode parse_object(size_t depth) {
        advance(); // '{'
        skip_whitespace();
        JsonNode::Object obj;
        if (!eof() && peek() == '}') {
            advance();
            return JsonNode(std::move(obj));
        }
        while (true) {
            skip_whitespace();
            if (eof() || peek() != '"') {
                fail("expected double-quoted string key in JSON object");
            }
            std::string key = parse_string();
            skip_whitespace();
            if (eof() || peek() != ':') {
                fail("expected ':' after key in JSON object");
            }
            advance(); // ':'
            JsonNode val = parse_value(depth);
            obj[std::move(key)] = std::move(val);
            skip_whitespace();
            if (eof()) {
                fail("unterminated JSON object, expected ',' or '}'");
            }
            char c = peek();
            if (c == '}') {
                advance();
                break;
            }
            if (c != ',') {
                fail(std::string("expected ',' or '}' in JSON object, found '") + c + "'");
            }
            advance(); // ','
            skip_whitespace();
            if (!eof() && peek() == '}') {
                fail("trailing comma is not permitted in JSON object");
            }
        }
        return JsonNode(std::move(obj));
    }
};

/// Serializes a JsonNode to compact or pretty-printed JSON string.
inline void serialize_node(const JsonNode& node, std::string& out, int indent, int current_depth) {
    if (node.is_null()) {
        out += "null";
    } else if (node.is_bool()) {
        out += std::get<bool>(node.value) ? "true" : "false";
    } else if (node.is_int()) {
        out += std::to_string(std::get<int64_t>(node.value));
    } else if (node.is_float()) {
        out += format_double(std::get<double>(node.value));
    } else if (node.is_string()) {
        out += escape_json_string(std::get<std::string>(node.value));
    } else if (node.is_array()) {
        const auto& arr = std::get<JsonNode::Array>(node.value);
        if (arr.empty()) {
            out += "[]";
            return;
        }
        out.push_back('[');
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) out.push_back(',');
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>((current_depth + 1) * indent), ' ');
            }
            serialize_node(arr[i], out, indent, current_depth + 1);
        }
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(current_depth * indent), ' ');
        }
        out.push_back(']');
    } else if (node.is_object()) {
        const auto& obj = std::get<JsonNode::Object>(node.value);
        if (obj.empty()) {
            out += "{}";
            return;
        }
        out.push_back('{');
        size_t i = 0;
        for (const auto& [k, v] : obj) {
            if (i > 0) out.push_back(',');
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>((current_depth + 1) * indent), ' ');
            }
            out += escape_json_string(k);
            out.push_back(':');
            if (indent > 0) {
                out.push_back(' ');
            }
            serialize_node(v, out, indent, current_depth + 1);
            ++i;
        }
        if (indent > 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(current_depth * indent), ' ');
        }
        out.push_back('}');
    }
}

[[nodiscard]] inline std::string dump_node(const JsonNode& node, int indent = 0) {
    std::string out;
    serialize_node(node, out, indent, 0);
    return out;
}

/// Splits a dot-path (e.g. "server.host" or "users.0.name") into segments.
/// An empty path "" returns an empty vector (representing the root node).
[[nodiscard]] inline std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> segments;
    if (path.empty()) {
        return segments;
    }
    size_t start = 0;
    while (start <= path.size()) {
        size_t dot = path.find('.', start);
        if (dot == std::string_view::npos) {
            segments.push_back(path.substr(start));
            break;
        }
        segments.push_back(path.substr(start, dot - start));
        start = dot + 1;
    }
    return segments;
}

/// Parses a non-negative array index from a path segment.
[[nodiscard]] inline std::optional<size_t> parse_index_segment(std::string_view seg) noexcept {
    if (seg.empty()) return std::nullopt;
    if (seg.size() > 1 && seg[0] == '0') return std::nullopt;
    size_t idx = 0;
    auto [ptr, ec] = std::from_chars(seg.data(), seg.data() + seg.size(), idx);
    if (ec != std::errc{} || ptr != seg.data() + seg.size()) {
        return std::nullopt;
    }
    return idx;
}

/// Navigates a const JsonNode tree using dot-separated path segments.
/// Returns nullptr if any key or array index along the path does not exist.
[[nodiscard]] inline const JsonNode* resolve_path_ptr(const JsonNode& root, std::string_view path) noexcept {
    if (path.empty()) {
        return &root;
    }
    auto segments = split_path(path);
    const JsonNode* curr = &root;
    for (std::string_view seg : segments) {
        if (curr->is_object()) {
            const auto& obj = std::get<JsonNode::Object>(curr->value);
            auto it = obj.find(std::string(seg));
            if (it == obj.end()) {
                return nullptr;
            }
            curr = &it->second;
        } else if (curr->is_array()) {
            const auto& arr = std::get<JsonNode::Array>(curr->value);
            auto idx_opt = parse_index_segment(seg);
            if (!idx_opt.has_value() || *idx_opt >= arr.size()) {
                return nullptr;
            }
            curr = &arr[*idx_opt];
        } else {
            return nullptr;
        }
    }
    return curr;
}

/// Navigates and resolves a path, throwing keet::error("JsonError", ...) if missing.
[[nodiscard]] inline const JsonNode& resolve_path_checked(
    const JsonNode& root,
    std::string_view path,
    const char* fn_name) {
    const JsonNode* target = resolve_path_ptr(root, path);
    if (target == nullptr) {
        throw keet::error(
            "JsonError",
            std::string(fn_name) + ": path '" + std::string(path) + "' not found in JSON document"
        );
    }
    return *target;
}

/// Mutates or inserts a value at a dot-path inside root.
/// Automatically creates intermediate objects when traversing missing object keys.
inline void set_at_path(
    JsonNode& root,
    std::string_view path,
    JsonNode new_val,
    const char* fn_name) {
    if (path.empty()) {
        root = std::move(new_val);
        return;
    }
    auto segments = split_path(path);
    JsonNode* curr = &root;
    for (size_t i = 0; i < segments.size(); ++i) {
        std::string_view seg = segments[i];
        if (seg.empty()) {
            throw keet::error("JsonError", std::string(fn_name) + ": empty path segment in '" + std::string(path) + "'");
        }
        bool is_last = (i + 1 == segments.size());
        if (curr->is_object()) {
            auto& obj = std::get<JsonNode::Object>(curr->value);
            std::string key(seg);
            if (is_last) {
                obj[std::move(key)] = std::move(new_val);
                return;
            }
            auto it = obj.find(key);
            if (it == obj.end()) {
                it = obj.emplace(std::move(key), JsonNode(JsonNode::Object{})).first;
            } else if (!it->second.is_object() && !it->second.is_array()) {
                throw keet::error(
                    "JsonError",
                    std::string(fn_name) + ": cannot traverse through non-container at segment '" +
                        std::string(seg) + "' in path '" + std::string(path) + "'"
                );
            }
            curr = &it->second;
        } else if (curr->is_array()) {
            auto& arr = std::get<JsonNode::Array>(curr->value);
            auto idx_opt = parse_index_segment(seg);
            if (!idx_opt.has_value()) {
                throw keet::error(
                    "JsonError",
                    std::string(fn_name) + ": invalid array index '" + std::string(seg) +
                        "' in path '" + std::string(path) + "'"
                );
            }
            size_t idx = *idx_opt;
            if (is_last) {
                if (idx < arr.size()) {
                    arr[idx] = std::move(new_val);
                } else if (idx == arr.size()) {
                    arr.push_back(std::move(new_val));
                } else {
                    throw keet::error(
                        "JsonError",
                        std::string(fn_name) + ": array index " + std::to_string(idx) +
                            " out of bounds (size " + std::to_string(arr.size()) + ")"
                    );
                }
                return;
            }
            if (idx >= arr.size()) {
                throw keet::error(
                    "JsonError",
                    std::string(fn_name) + ": array index " + std::to_string(idx) +
                        " out of bounds (size " + std::to_string(arr.size()) + ")"
                );
            }
            curr = &arr[idx];
        } else {
            throw keet::error(
                "JsonError",
                std::string(fn_name) + ": cannot set key on non-container JSON root/node for path '" +
                    std::string(path) + "'"
            );
        }
    }
}

// Type traits for polymorphic json::stringify
template <typename T> struct is_vector : std::false_type {};
template <typename T, typename A> struct is_vector<std::vector<T, A>> : std::true_type {};
template <typename T> inline constexpr bool is_vector_v = is_vector<T>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};
template <typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_variant : std::false_type {};
template <typename... Ts> struct is_variant<std::variant<Ts...>> : std::true_type {};
template <typename T> inline constexpr bool is_variant_v = is_variant<T>::value;

template <typename T> struct is_unordered_map : std::false_type {};
template <typename K, typename V, typename H, typename E, typename A>
struct is_unordered_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template <typename T> inline constexpr bool is_unordered_map_v = is_unordered_map<T>::value;

template <typename T> struct is_ordered_map : std::false_type {};
template <typename K, typename V, typename C, typename A>
struct is_ordered_map<std::map<K, V, C, A>> : std::true_type {};
template <typename T> inline constexpr bool is_ordered_map_v = is_ordered_map<T>::value;

template <typename T> struct is_deep_copy_ptr : std::false_type {};
template <typename T> struct is_deep_copy_ptr<keet::deep_copy_ptr<T>> : std::true_type {};
template <typename T> inline constexpr bool is_deep_copy_ptr_v = is_deep_copy_ptr<T>::value;

/// Converts a JsonNode into a string suitable for `parseMap` / `parseArray`:
/// Strings are returned unescaped directly; primitives/containers are serialized to JSON text.
[[nodiscard]] inline std::string node_to_element_string(const JsonNode& node) {
    if (node.is_string()) {
        return std::get<std::string>(node.value);
    }
    return dump_node(node, 0);
}

} // namespace detail

// ============================================================================
// 1. Validation & Type Inspection
// ============================================================================

/// Returns true if text is strictly valid RFC 8259 JSON; false otherwise. Non-throwing.
[[nodiscard]] inline bool isValid(const std::string& text) noexcept {
    try {
        detail::Parser p(text);
        (void)p.parse_document();
        return true;
    } catch (...) {
        return false;
    }
}

/// Returns the JSON value category: "object", "array", "string", "number", "bool", "null", or "invalid".
[[nodiscard]] inline std::string typeOf(const std::string& text) noexcept {
    try {
        detail::Parser p(text);
        detail::JsonNode node = p.parse_document();
        return std::string(node.type_name());
    } catch (...) {
        return "invalid";
    }
}

/// Returns true if text is a valid JSON object ({...}). Non-throwing.
[[nodiscard]] inline bool isObject(const std::string& text) noexcept {
    try {
        detail::Parser p(text);
        return p.parse_document().is_object();
    } catch (...) {
        return false;
    }
}

/// Returns true if text is a valid JSON array ([...]). Non-throwing.
[[nodiscard]] inline bool isArray(const std::string& text) noexcept {
    try {
        detail::Parser p(text);
        return p.parse_document().is_array();
    } catch (...) {
        return false;
    }
}

/// Returns the entry/element count of a JSON object or array. Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline int64_t size(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (root.is_object()) {
        return static_cast<int64_t>(std::get<detail::JsonNode::Object>(root.value).size());
    }
    if (root.is_array()) {
        return static_cast<int64_t>(std::get<detail::JsonNode::Array>(root.value).size());
    }
    throw keet::error(
        "JsonError",
        "size: expected JSON object or array, got " + std::string(root.type_name())
    );
}

/// Returns a sorted array of top-level keys of a JSON object. Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline std::vector<std::string> keys(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_object()) {
        throw keet::error(
            "JsonError",
            "keys: expected JSON object, got " + std::string(root.type_name())
        );
    }
    const auto& obj = std::get<detail::JsonNode::Object>(root.value);
    std::vector<std::string> result;
    result.reserve(obj.size());
    for (const auto& [k, _] : obj) {
        result.push_back(k);
    }
    return result;
}

// ============================================================================
// 2. Dot-Path Queries & Typed Extraction
// ============================================================================

/// Returns true if text is valid JSON and path resolves to an existing key/index. Non-throwing.
[[nodiscard]] inline bool hasKey(const std::string& text, const std::string& path) noexcept {
    if (path.empty()) {
        return isValid(text);
    }
    try {
        detail::Parser p(text);
        detail::JsonNode root = p.parse_document();
        return detail::resolve_path_ptr(root, path) != nullptr;
    } catch (...) {
        return false;
    }
}

/// Returns true if text is valid JSON and path resolves to a JSON null literal. Non-throwing.
[[nodiscard]] inline bool isNull(const std::string& text, const std::string& path) noexcept {
    try {
        detail::Parser p(text);
        detail::JsonNode root = p.parse_document();
        const detail::JsonNode* target = detail::resolve_path_ptr(root, path);
        return target != nullptr && target->is_null();
    } catch (...) {
        return false;
    }
}

/// Extracts the unescaped string at path. Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline std::string getString(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getString");
    if (!target.is_string()) {
        throw keet::error(
            "JsonError",
            "getString: value at path '" + path + "' is " + std::string(target.type_name()) + ", expected string"
        );
    }
    return std::get<std::string>(target.value);
}

/// Extracts an exact 64-bit integer at path. Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline int64_t getInt(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getInt");
    if (target.is_int()) {
        return std::get<int64_t>(target.value);
    }
    throw keet::error(
        "JsonError",
        "getInt: value at path '" + path + "' is not an exact integer (got " + std::string(target.type_name()) + ")"
    );
}

/// Extracts a double at path (accepts both integer and floating-point JSON numbers).
/// Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline double getFloat(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getFloat");
    if (target.is_float()) {
        return std::get<double>(target.value);
    }
    if (target.is_int()) {
        return static_cast<double>(std::get<int64_t>(target.value));
    }
    throw keet::error(
        "JsonError",
        "getFloat: value at path '" + path + "' is " + std::string(target.type_name()) + ", expected number"
    );
}

/// Extracts a boolean at path. Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline bool getBool(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getBool");
    if (!target.is_bool()) {
        throw keet::error(
            "JsonError",
            "getBool: value at path '" + path + "' is " + std::string(target.type_name()) + ", expected bool"
        );
    }
    return std::get<bool>(target.value);
}

/// Extracts a JSON array at path, returning each element as a string (unescaped if string, or serialized JSON).
/// Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline std::vector<std::string> getArray(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getArray");
    if (!target.is_array()) {
        throw keet::error(
            "JsonError",
            "getArray: value at path '" + path + "' is " + std::string(target.type_name()) + ", expected array"
        );
    }
    const auto& arr = std::get<detail::JsonNode::Array>(target.value);
    std::vector<std::string> result;
    result.reserve(arr.size());
    for (const auto& elem : arr) {
        result.push_back(detail::node_to_element_string(elem));
    }
    return result;
}

/// Extracts a nested JSON object at path as a serialized JSON string.
/// Throws keet::error("JsonError", ...) on failure.
[[nodiscard]] inline std::string getObject(const std::string& text, const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    const detail::JsonNode& target = detail::resolve_path_checked(root, path, "getObject");
    if (!target.is_object()) {
        throw keet::error(
            "JsonError",
            "getObject: value at path '" + path + "' is " + std::string(target.type_name()) + ", expected object"
        );
    }
    return detail::dump_node(target, 0);
}

// ============================================================================
// 3. Non-Throwing Fallback Extractors (*Or)
// ============================================================================

/// Returns getString(text, path) on success, or defaultVal on any error. Non-throwing.
[[nodiscard]] inline std::string getStringOr(
    const std::string& text,
    const std::string& path,
    const std::string& defaultVal) noexcept {
    try {
        return getString(text, path);
    } catch (...) {
        return defaultVal;
    }
}

/// Returns getInt(text, path) on success, or defaultVal on any error. Non-throwing.
[[nodiscard]] inline int64_t getIntOr(
    const std::string& text,
    const std::string& path,
    int64_t defaultVal) noexcept {
    try {
        return getInt(text, path);
    } catch (...) {
        return defaultVal;
    }
}

/// Returns getFloat(text, path) on success, or defaultVal on any error. Non-throwing.
[[nodiscard]] inline double getFloatOr(
    const std::string& text,
    const std::string& path,
    double defaultVal) noexcept {
    try {
        return getFloat(text, path);
    } catch (...) {
        return defaultVal;
    }
}

/// Returns getBool(text, path) on success, or defaultVal on any error. Non-throwing.
[[nodiscard]] inline bool getBoolOr(
    const std::string& text,
    const std::string& path,
    bool defaultVal) noexcept {
    try {
        return getBool(text, path);
    } catch (...) {
        return defaultVal;
    }
}

// ============================================================================
// 4. Typed Collection Parsers
// ============================================================================

/// Parses a JSON object into a map<string, string>. String values are unescaped directly;
/// non-string values are converted to their canonical JSON string representation.
/// Throws keet::error("JsonError", ...) if text is not a valid JSON object.
[[nodiscard]] inline std::unordered_map<std::string, std::string> parseMap(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_object()) {
        throw keet::error(
            "JsonError",
            "parseMap: expected JSON object, got " + std::string(root.type_name())
        );
    }
    const auto& obj = std::get<detail::JsonNode::Object>(root.value);
    std::unordered_map<std::string, std::string> result;
    result.reserve(obj.size());
    for (const auto& [k, v] : obj) {
        result.emplace(k, detail::node_to_element_string(v));
    }
    return result;
}

/// Parses a JSON array into an array<string>. String elements are unescaped directly;
/// non-string elements are converted to their canonical JSON string representation.
/// Throws keet::error("JsonError", ...) if text is not a valid JSON array.
[[nodiscard]] inline std::vector<std::string> parseArray(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_array()) {
        throw keet::error(
            "JsonError",
            "parseArray: expected JSON array, got " + std::string(root.type_name())
        );
    }
    const auto& arr = std::get<detail::JsonNode::Array>(root.value);
    std::vector<std::string> result;
    result.reserve(arr.size());
    for (const auto& elem : arr) {
        result.push_back(detail::node_to_element_string(elem));
    }
    return result;
}

/// Parses a homogeneous JSON array of exact integers into array<int>.
/// Throws keet::error("JsonError", ...) if not an array or if any element is not an integer.
[[nodiscard]] inline std::vector<int64_t> parseIntArray(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_array()) {
        throw keet::error(
            "JsonError",
            "parseIntArray: expected JSON array, got " + std::string(root.type_name())
        );
    }
    const auto& arr = std::get<detail::JsonNode::Array>(root.value);
    std::vector<int64_t> result;
    result.reserve(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        if (!arr[i].is_int()) {
            throw keet::error(
                "JsonError",
                "parseIntArray: element at index " + std::to_string(i) + " is not an exact integer"
            );
        }
        result.push_back(std::get<int64_t>(arr[i].value));
    }
    return result;
}

/// Parses a homogeneous JSON array of numbers into array<float>.
/// Throws keet::error("JsonError", ...) if not an array or if any element is not a number.
[[nodiscard]] inline std::vector<double> parseFloatArray(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_array()) {
        throw keet::error(
            "JsonError",
            "parseFloatArray: expected JSON array, got " + std::string(root.type_name())
        );
    }
    const auto& arr = std::get<detail::JsonNode::Array>(root.value);
    std::vector<double> result;
    result.reserve(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].is_float()) {
            result.push_back(std::get<double>(arr[i].value));
        } else if (arr[i].is_int()) {
            result.push_back(static_cast<double>(std::get<int64_t>(arr[i].value)));
        } else {
            throw keet::error(
                "JsonError",
                "parseFloatArray: element at index " + std::to_string(i) + " is not a number"
            );
        }
    }
    return result;
}

/// Parses a homogeneous JSON array of booleans into array<bool> (std::vector<bool>).
/// Throws keet::error("JsonError", ...) if not an array or if any element is not a boolean.
[[nodiscard]] inline std::vector<bool> parseBoolArray(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    if (!root.is_array()) {
        throw keet::error(
            "JsonError",
            "parseBoolArray: expected JSON array, got " + std::string(root.type_name())
        );
    }
    const auto& arr = std::get<detail::JsonNode::Array>(root.value);
    std::vector<bool> result;
    result.reserve(arr.size());
    for (size_t i = 0; i < arr.size(); ++i) {
        if (!arr[i].is_bool()) {
            throw keet::error(
                "JsonError",
                "parseBoolArray: element at index " + std::to_string(i) + " is not a boolean"
            );
        }
        result.push_back(std::get<bool>(arr[i].value));
    }
    return result;
}

// ============================================================================
// 5. Immutable Object & Array Builders / Mutation Helpers
// ============================================================================

/// Returns an empty JSON object literal "{}".
[[nodiscard]] inline std::string emptyObject() {
    return "{}";
}

/// Returns an empty JSON array literal "[]".
[[nodiscard]] inline std::string emptyArray() {
    return "[]";
}

/// Sets path to a string value in text and returns the updated JSON string.
[[nodiscard]] inline std::string setString(
    const std::string& text,
    const std::string& path,
    const std::string& value) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    detail::set_at_path(root, path, detail::JsonNode(value), "setString");
    return detail::dump_node(root, 0);
}

/// Sets path to an integer value in text and returns the updated JSON string.
[[nodiscard]] inline std::string setInt(
    const std::string& text,
    const std::string& path,
    int64_t value) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    detail::set_at_path(root, path, detail::JsonNode(value), "setInt");
    return detail::dump_node(root, 0);
}

/// Sets path to a finite float value in text and returns the updated JSON string.
[[nodiscard]] inline std::string setFloat(
    const std::string& text,
    const std::string& path,
    double value) {
    if (!std::isfinite(value)) {
        throw keet::error("JsonError", "setFloat: cannot serialize NaN or Infinity to JSON");
    }
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    detail::set_at_path(root, path, detail::JsonNode(value), "setFloat");
    return detail::dump_node(root, 0);
}

/// Sets path to a boolean value in text and returns the updated JSON string.
[[nodiscard]] inline std::string setBool(
    const std::string& text,
    const std::string& path,
    bool value) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    detail::set_at_path(root, path, detail::JsonNode(value), "setBool");
    return detail::dump_node(root, 0);
}

/// Sets path to JSON null in text and returns the updated JSON string.
[[nodiscard]] inline std::string setNull(
    const std::string& text,
    const std::string& path) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    detail::set_at_path(root, path, detail::JsonNode(std::monostate{}), "setNull");
    return detail::dump_node(root, 0);
}

/// Parses rawJson as a JSON value and embeds it at path inside text.
[[nodiscard]] inline std::string setRaw(
    const std::string& text,
    const std::string& path,
    const std::string& rawJson) {
    detail::Parser p_root(text);
    detail::JsonNode root = p_root.parse_document();
    detail::Parser p_val(rawJson);
    detail::JsonNode child = p_val.parse_document();
    detail::set_at_path(root, path, std::move(child), "setRaw");
    return detail::dump_node(root, 0);
}

/// Removes the object key or array index at path and returns the updated JSON string.
/// If an object key does not exist, returns the validated JSON unchanged.
[[nodiscard]] inline std::string removeKey(
    const std::string& text,
    const std::string& path) {
    if (path.empty()) {
        throw keet::error("JsonError", "removeKey: path cannot be empty");
    }
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    auto segments = detail::split_path(path);
    detail::JsonNode* curr = &root;
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        std::string_view seg = segments[i];
        if (curr->is_object()) {
            auto& obj = std::get<detail::JsonNode::Object>(curr->value);
            auto it = obj.find(std::string(seg));
            if (it == obj.end()) {
                return detail::dump_node(root, 0);
            }
            curr = &it->second;
        } else if (curr->is_array()) {
            auto& arr = std::get<detail::JsonNode::Array>(curr->value);
            auto idx_opt = detail::parse_index_segment(seg);
            if (!idx_opt.has_value() || *idx_opt >= arr.size()) {
                return detail::dump_node(root, 0);
            }
            curr = &arr[*idx_opt];
        } else {
            return detail::dump_node(root, 0);
        }
    }

    std::string_view last = segments.back();
    if (curr->is_object()) {
        auto& obj = std::get<detail::JsonNode::Object>(curr->value);
        obj.erase(std::string(last));
    } else if (curr->is_array()) {
        auto& arr = std::get<detail::JsonNode::Array>(curr->value);
        auto idx_opt = detail::parse_index_segment(last);
        if (!idx_opt.has_value() || *idx_opt >= arr.size()) {
            throw keet::error(
                "JsonError",
                "removeKey: invalid or out-of-bounds array index '" + std::string(last) + "'"
            );
        }
        arr.erase(arr.begin() + static_cast<std::ptrdiff_t>(*idx_opt));
    } else {
        throw keet::error(
            "JsonError",
            "removeKey: target parent at path '" + path + "' is not an object or array"
        );
    }
    return detail::dump_node(root, 0);
}

// ============================================================================
// 6. Polymorphic Serialization, Formatting, Escaping & File I/O
// ============================================================================

/// Polymorphic serializer converting any native KeetScript value to an RFC 8259 JSON string.
template <typename T>
[[nodiscard]] std::string stringify(const T& value) {
    using DecayT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayT, std::monostate> || std::is_same_v<DecayT, std::nullptr_t>) {
        return "null";
    } else if constexpr (std::is_same_v<DecayT, bool> || std::is_same_v<DecayT, std::vector<bool>::reference> ||
                         std::is_same_v<DecayT, std::vector<bool>::const_reference>) {
        return static_cast<bool>(value) ? "true" : "false";
    } else if constexpr (std::is_same_v<DecayT, keet::BoolByte>) {
        return static_cast<bool>(value) ? "true" : "false";
    } else if constexpr (std::is_same_v<DecayT, char>) {
        char buf[2] = {value, '\0'};
        return detail::escape_json_string(std::string_view(buf, 1));
    } else if constexpr (std::is_integral_v<DecayT>) {
        return std::to_string(value);
    } else if constexpr (std::is_floating_point_v<DecayT>) {
        return detail::format_double(static_cast<double>(value), "stringify");
    } else if constexpr (std::is_same_v<DecayT, std::string> || std::is_same_v<DecayT, std::string_view>) {
        return detail::escape_json_string(value);
    } else if constexpr (std::is_convertible_v<DecayT, std::string_view>) {
        return detail::escape_json_string(std::string_view(value));
    } else if constexpr (detail::is_optional_v<DecayT>) {
        if (!value.has_value()) {
            return "null";
        }
        return stringify(*value);
    } else if constexpr (detail::is_deep_copy_ptr_v<DecayT>) {
        if (!value.hasValue()) {
            return "null";
        }
        return stringify(*value);
    } else if constexpr (detail::is_variant_v<DecayT>) {
        return std::visit([](const auto& inner) -> std::string {
            return stringify(inner);
        }, value);
    } else if constexpr (detail::is_vector_v<DecayT>) {
        std::string out = "[";
        for (size_t i = 0; i < value.size(); ++i) {
            if (i > 0) out.push_back(',');
            if constexpr (std::is_same_v<DecayT, std::vector<bool>>) {
                out += static_cast<bool>(value[i]) ? "true" : "false";
            } else {
                out += stringify(value[i]);
            }
        }
        out.push_back(']');
        return out;
    } else if constexpr (detail::is_unordered_map_v<DecayT>) {
        // Sort keys lexicographically for deterministic JSON output
        std::vector<std::string> sorted_keys;
        sorted_keys.reserve(value.size());
        for (const auto& [k, _] : value) {
            sorted_keys.push_back(k);
        }
        std::sort(sorted_keys.begin(), sorted_keys.end());
        std::string out = "{";
        for (size_t i = 0; i < sorted_keys.size(); ++i) {
            if (i > 0) out.push_back(',');
            const std::string& k = sorted_keys[i];
            out += detail::escape_json_string(k);
            out.push_back(':');
            out += stringify(value.at(k));
        }
        out.push_back('}');
        return out;
    } else if constexpr (detail::is_ordered_map_v<DecayT>) {
        std::string out = "{";
        size_t i = 0;
        for (const auto& [k, v] : value) {
            if (i > 0) out.push_back(',');
            out += detail::escape_json_string(k);
            out.push_back(':');
            out += stringify(v);
            ++i;
        }
        out.push_back('}');
        return out;
    } else {
        static_assert(sizeof(T) == 0, "keet::json::stringify: unsupported type for JSON serialization");
    }
}

/// Validates text and formats it with indent spaces per nesting level (0 <= indent <= 16).
/// Throws keet::error("JsonError", ...) on invalid JSON or out-of-range indent.
[[nodiscard]] inline std::string format(const std::string& text, int64_t indent) {
    if (indent < 0 || indent > 16) {
        throw keet::error(
            "JsonError",
            "format: indent must be between 0 and 16, got " + std::to_string(indent)
        );
    }
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    return detail::dump_node(root, static_cast<int>(indent));
}

/// Validates text and removes all insignificant whitespace. Throws keet::error("JsonError", ...) on invalid JSON.
[[nodiscard]] inline std::string minify(const std::string& text) {
    detail::Parser p(text);
    detail::JsonNode root = p.parse_document();
    return detail::dump_node(root, 0);
}

/// Escapes a raw string into a double-quoted JSON string literal ("..."). Non-throwing.
[[nodiscard]] inline std::string escape(const std::string& str) {
    return detail::escape_json_string(str);
}

/// Unescapes a JSON string literal (with or without surrounding double quotes).
/// Throws keet::error("JsonError", ...) on invalid escape sequences.
[[nodiscard]] inline std::string unescape(const std::string& str) {
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        detail::Parser p(str);
        return p.parse_string_literal_only();
    }
    std::string wrapped;
    wrapped.reserve(str.size() + 2);
    wrapped.push_back('"');
    wrapped += str;
    wrapped.push_back('"');
    detail::Parser p(wrapped);
    return p.parse_string_literal_only();
}

/// Reads UTF-8 file at path, validates that its contents are valid RFC 8259 JSON,
/// and returns the canonical minified or validated JSON string.
/// Throws keet::error("JsonError", ...) if file cannot be read or contains invalid JSON.
[[nodiscard]] inline std::string readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        throw keet::error("JsonError", "readFile: cannot open JSON file for reading: '" + path + "'");
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad()) {
        throw keet::error("JsonError", "readFile: I/O error while reading JSON file: '" + path + "'");
    }
    detail::Parser p(content);
    detail::JsonNode root = p.parse_document();
    return detail::dump_node(root, 0);
}

/// Validates text as JSON, formats it with indent spaces (0 for minified, 1..16 for pretty-printed),
/// and writes it to path. Returns true on success; throws keet::error("JsonError", ...) on failure.
inline bool writeFile(const std::string& path, const std::string& text, int64_t indent) {
    std::string formatted = format(text, indent);
    if (indent > 0) {
        formatted.push_back('\n');
    }
    std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        throw keet::error("JsonError", "writeFile: cannot open file for writing: '" + path + "'");
    }
    ofs.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    if (!ofs.good()) {
        throw keet::error("JsonError", "writeFile: failed to write JSON content to file: '" + path + "'");
    }
    ofs.flush();
    return true;
}

} // namespace keet::json
