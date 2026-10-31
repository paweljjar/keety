#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <keet/error.hpp>

namespace keet {

/// Deterministic 64-bit integer arithmetic with two's-complement wraparound guarantees.
/// KeetScript guarantees defined two's-complement overflow without C++ undefined behavior (UB),
/// conforming to Section 'Numeric types' and Test 1.1/1.2 of Conformance Test Specification.

[[nodiscard]] constexpr int64_t add_wrap(int64_t a, int64_t b) noexcept {
    // Standard unsigned arithmetic wraps deterministically in two's complement
    return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

[[nodiscard]] constexpr int64_t sub_wrap(int64_t a, int64_t b) noexcept {
    return static_cast<int64_t>(static_cast<uint64_t>(a) - static_cast<uint64_t>(b));
}

[[nodiscard]] constexpr int64_t mul_wrap(int64_t a, int64_t b) noexcept {
    return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

[[nodiscard]] constexpr int64_t neg_wrap(int64_t a) noexcept {
    // Negating -9223372036854775808 (INT64_MIN) wraps around to INT64_MIN
    return static_cast<int64_t>(0ULL - static_cast<uint64_t>(a));
}

/// Fallible integer division: raises keet::error("DivisionByZero") if b == 0,
/// and handles INT64_MIN / -1 wrapping deterministically to INT64_MIN.
template <typename T, typename U>
[[nodiscard]] inline auto div_safe(T a, U b) {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        if (b == 0) {
            throw error("DivisionByZero", "Integer division by zero");
        }
        if (a == std::numeric_limits<T>::min() && b == -1) {
            return static_cast<int64_t>(std::numeric_limits<T>::min());
        }
        return static_cast<int64_t>(a / b);
    } else {
        return a / b;
    }
}

/// Fallible integer modulo: raises keet::error("DivisionByZero") if b == 0.
template <typename T, typename U>
[[nodiscard]] inline auto mod_safe(T a, U b) {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        if (b == 0) {
            throw error("DivisionByZero", "Integer modulo by zero");
        }
        if (a == std::numeric_limits<T>::min() && b == -1) {
            return static_cast<int64_t>(0);
        }
        return static_cast<int64_t>(a % b);
    } else {
        return std::fmod(a, b);
    }
}

/// Safe float-to-integer conversion with NaN and range checking.
template <typename T>
[[nodiscard]] inline int64_t to_int(T v) {
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(v)) {
            throw error("InvalidCast", "Cannot convert NaN float to integer");
        }
        if (v < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
            v > static_cast<double>(std::numeric_limits<int64_t>::max())) {
            throw error("OverflowError", "Float conversion out of 64-bit integer range");
        }
        return static_cast<int64_t>(v);
    } else {
        return static_cast<int64_t>(v);
    }
}

/// Clamps value between min and max bounds.
template <typename T, typename MinT, typename MaxT>
[[nodiscard]] inline auto clamp(T val, MinT min_val, MaxT max_val) noexcept {
    if (val < min_val) return static_cast<decltype(val + min_val + max_val)>(min_val);
    if (val > max_val) return static_cast<decltype(val + min_val + max_val)>(max_val);
    return static_cast<decltype(val + min_val + max_val)>(val);
}

} // namespace keet
