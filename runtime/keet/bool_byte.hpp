#pragma once

#include <cstdint>

namespace keet {

/// 1-byte boolean representation avoiding the space/proxy-reference issues
/// of std::vector<bool> specialization in generated C++17 translation units.
/// Conforms to Section 4.1 of Keety Compiler Architecture Specification.
struct BoolByte {
    uint8_t val{0};

    constexpr BoolByte() noexcept : val(0) {}
    constexpr BoolByte(bool b) noexcept : val(b ? 1 : 0) {}

    // Implicit conversion to bool for transparent conditional checks
    [[nodiscard]] constexpr operator bool() const noexcept {
        return val != 0;
    }

    constexpr BoolByte& operator=(bool b) noexcept {
        val = b ? 1 : 0;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const BoolByte& other) const noexcept {
        return (val != 0) == (other.val != 0);
    }

    [[nodiscard]] constexpr bool operator==(bool b) const noexcept {
        return (val != 0) == b;
    }

    [[nodiscard]] constexpr bool operator!=(const BoolByte& other) const noexcept {
        return (val != 0) != (other.val != 0);
    }

    [[nodiscard]] constexpr bool operator<(const BoolByte& other) const noexcept {
        return (val != 0) < (other.val != 0);
    }

    [[nodiscard]] constexpr bool operator<=(const BoolByte& other) const noexcept {
        return (val != 0) <= (other.val != 0);
    }

    [[nodiscard]] constexpr bool operator>(const BoolByte& other) const noexcept {
        return (val != 0) > (other.val != 0);
    }

    [[nodiscard]] constexpr bool operator>=(const BoolByte& other) const noexcept {
        return (val != 0) >= (other.val != 0);
    }
};

static_assert(sizeof(BoolByte) == 1, "BoolByte must be exactly 1 byte in size");

} // namespace keet
