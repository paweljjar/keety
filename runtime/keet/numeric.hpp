#pragma once

#include <cstdint>
#include <limits>
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
[[nodiscard]] inline int64_t div_safe(int64_t a, int64_t b) {
    if (b == 0) {
        throw error("DivisionByZero", "Integer division by zero");
    }
    // Handle the single overflow case: INT64_MIN / -1 produces INT64_MIN in two's complement
    if (a == std::numeric_limits<int64_t>::min() && b == -1) {
        return std::numeric_limits<int64_t>::min();
    }
    return a / b;
}

/// Fallible integer modulo: raises keet::error("DivisionByZero") if b == 0.
[[nodiscard]] inline int64_t mod_safe(int64_t a, int64_t b) {
    if (b == 0) {
        throw error("DivisionByZero", "Integer modulo by zero");
    }
    if (a == std::numeric_limits<int64_t>::min() && b == -1) {
        return 0;
    }
    return a % b;
}

} // namespace keet
