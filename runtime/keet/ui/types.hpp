#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace keet::ui {

/// Color representation conforming to Keet dot-syntax (Color.Blue, etc.)
enum class Color : uint8_t {
    Default = 0,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite
};

/// Border styling
enum class BorderStyle : uint8_t {
    None = 0,
    Single,
    Double,
    Rounded,
    Heavy
};

/// Text font style
enum class FontStyle : uint8_t {
    Normal = 0,
    Bold = 1 << 0,
    Dim = 1 << 1,
    Italic = 1 << 2,
    Underline = 1 << 3,
    Reverse = 1 << 4
};

inline constexpr FontStyle operator|(FontStyle a, FontStyle b) noexcept {
    return static_cast<FontStyle>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr bool operator&(FontStyle a, FontStyle b) noexcept {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/// Sizing mode for flexbox layout
enum class SizeMode : uint8_t {
    Auto,
    Fixed,
    Percent
};

struct Dimension {
    SizeMode mode{SizeMode::Auto};
    int value{0}; // integer character cells or percentage value (0-100)

    static constexpr Dimension Auto() noexcept { return {SizeMode::Auto, 0}; }
    static constexpr Dimension Cells(int c) noexcept { return {SizeMode::Fixed, c}; }
    static constexpr Dimension Percent(int p) noexcept { return {SizeMode::Percent, p}; }

    constexpr bool operator==(const Dimension&) const noexcept = default;
};

} // namespace keet::ui

