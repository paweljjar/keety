#pragma once

#include <cstdint>

namespace keet::ui {

struct Point {
    int x{0};
    int y{0};

    constexpr bool operator==(const Point&) const noexcept = default;
};

struct Size {
    int width{0};
    int height{0};

    constexpr bool operator==(const Size&) const noexcept = default;
};

struct Rect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    [[nodiscard]] constexpr bool contains(Point p) const noexcept {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }

    [[nodiscard]] constexpr bool contains(int px, int py) const noexcept {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    constexpr bool operator==(const Rect&) const noexcept = default;
};

struct Edges {
    int top{0};
    int right{0};
    int bottom{0};
    int left{0};

    static constexpr Edges All(int val) noexcept {
        return {val, val, val, val};
    }

    static constexpr Edges Symmetric(int vertical, int horizontal) noexcept {
        return {vertical, horizontal, vertical, horizontal};
    }

    [[nodiscard]] constexpr int horizontal() const noexcept { return left + right; }
    [[nodiscard]] constexpr int vertical() const noexcept { return top + bottom; }

    constexpr bool operator==(const Edges&) const noexcept = default;
};

} // namespace keet::ui

