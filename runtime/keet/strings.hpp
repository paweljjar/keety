#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#if defined(__GNUC__) || defined(__clang__)
#define KEET_ALWAYS_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define KEET_ALWAYS_INLINE inline __forceinline
#else
#define KEET_ALWAYS_INLINE inline
#endif

namespace keet {

/// Non-throwing, zero-unwinding 64-bit integer parser.
/// Returns std::variant<int64_t, std::monostate> holding int64_t on success,
/// or std::monostate (null) on failure.
[[nodiscard]] KEET_ALWAYS_INLINE std::variant<int64_t, std::monostate> try_int(const char* data, size_t size) noexcept {
    const char* first = data;
    const char* last = data + size;

    if (first < last && (*first == ' ' || *first == '\t' || *first == '+')) {
        while (first < last && (*first == ' ' || *first == '\t')) {
            ++first;
        }
        if (first < last && *first == '+') {
            ++first;
        }
    }

    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(first, last, val);
    if (ec == std::errc{} && ptr == last) {
        return val;
    }
    return std::monostate{};
}

[[nodiscard]] KEET_ALWAYS_INLINE std::variant<int64_t, std::monostate> try_int(const std::string& s) noexcept {
    return try_int(s.data(), s.size());
}

[[nodiscard]] KEET_ALWAYS_INLINE std::variant<int64_t, std::monostate> try_int(std::string_view s) noexcept {
    return try_int(s.data(), s.size());
}

/// Trims leading and trailing ASCII whitespace.
[[nodiscard]] inline std::string_view trim(std::string_view s) noexcept {
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

} // namespace keet
