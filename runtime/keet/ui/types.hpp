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
    Percent,
    Pixels
};

/// 32-bit RGBA color representation for high-fidelity 2D desktop rendering
struct RgbaColor {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    static constexpr RgbaColor fromRgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept {
        return RgbaColor{red, green, blue, alpha};
    }

    static constexpr RgbaColor fromHex(uint32_t hex, uint8_t alpha = 255) noexcept {
        return RgbaColor{
            static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF),
            static_cast<uint8_t>(hex & 0xFF),
            alpha
        };
    }

    static RgbaColor parseHex(std::string_view hexStr) noexcept {
        if (hexStr.empty()) return RgbaColor{};
        if (hexStr.front() == '#') hexStr.remove_prefix(1);

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        if (hexStr.size() == 6 || hexStr.size() == 8) {
            uint32_t val = 0;
            for (size_t i = 0; i < hexStr.size(); ++i) {
                int h = hexVal(hexStr[i]);
                if (h < 0) return RgbaColor{};
                val = (val << 4) | static_cast<uint32_t>(h);
            }
            if (hexStr.size() == 6) {
                return fromHex(val, 255);
            } else {
                return RgbaColor{
                    static_cast<uint8_t>((val >> 24) & 0xFF),
                    static_cast<uint8_t>((val >> 16) & 0xFF),
                    static_cast<uint8_t>((val >> 8) & 0xFF),
                    static_cast<uint8_t>(val & 0xFF)
                };
            }
        }
        return RgbaColor{};
    }

    static constexpr RgbaColor fromThemeColor(Color c, bool isBackground = false) noexcept {
        switch (c) {
            case Color::Default:
                return isBackground ? RgbaColor{30, 30, 46, 255} : RgbaColor{205, 214, 244, 255};
            case Color::Black:
                return RgbaColor{17, 17, 27, 255};
            case Color::Red:
                return RgbaColor{243, 139, 168, 255};
            case Color::Green:
                return RgbaColor{166, 227, 161, 255};
            case Color::Yellow:
                return RgbaColor{249, 226, 175, 255};
            case Color::Blue:
                return RgbaColor{137, 180, 250, 255};
            case Color::Magenta:
                return RgbaColor{203, 166, 247, 255};
            case Color::Cyan:
                return RgbaColor{148, 226, 213, 255};
            case Color::White:
                return RgbaColor{205, 214, 244, 255};
            case Color::BrightBlack:
                return RgbaColor{88, 91, 112, 255};
            case Color::BrightRed:
                return RgbaColor{235, 111, 146, 255};
            case Color::BrightGreen:
                return RgbaColor{156, 207, 150, 255};
            case Color::BrightYellow:
                return RgbaColor{246, 193, 119, 255};
            case Color::BrightBlue:
                return RgbaColor{156, 207, 250, 255};
            case Color::BrightMagenta:
                return RgbaColor{218, 176, 250, 255};
            case Color::BrightCyan:
                return RgbaColor{170, 230, 220, 255};
            case Color::BrightWhite:
                return RgbaColor{255, 255, 255, 255};
        }
        return isBackground ? RgbaColor{30, 30, 46, 255} : RgbaColor{205, 214, 244, 255};
    }

    constexpr bool operator==(const RgbaColor&) const noexcept = default;
};

struct Dimension {
    SizeMode mode{SizeMode::Auto};
    int value{0}; // integer character cells, percentage value (0-100), or desktop pixels

    static constexpr Dimension Auto() noexcept { return {SizeMode::Auto, 0}; }
    static constexpr Dimension Cells(int c) noexcept { return {SizeMode::Fixed, c}; }
    static constexpr Dimension Percent(int p) noexcept { return {SizeMode::Percent, p}; }
    static constexpr Dimension Pixels(int px) noexcept { return {SizeMode::Pixels, px}; }

    [[nodiscard]] constexpr bool isAuto() const noexcept { return mode == SizeMode::Auto; }
    [[nodiscard]] constexpr bool isFixed() const noexcept { return mode == SizeMode::Fixed; }
    [[nodiscard]] constexpr bool isPercent() const noexcept { return mode == SizeMode::Percent; }
    [[nodiscard]] constexpr bool isPixels() const noexcept { return mode == SizeMode::Pixels; }

    constexpr bool operator==(const Dimension&) const noexcept = default;
};

} // namespace keet::ui

