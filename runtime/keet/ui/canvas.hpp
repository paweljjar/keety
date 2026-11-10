#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <keet/ui/font_data.hpp>
#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/types.hpp>
#include <keet/ui/utf8.hpp>
#include <keet/ui/font.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keet::ui {

/// 32-bit RGBA pixel memory buffer with dirty damage tracking
class FrameBuffer {
public:
    FrameBuffer(int width = 800, int height = 600, float dpiScale = 1.0f)
        : width_(std::max(1, width)), height_(std::max(1, height)), dpiScale_(dpiScale) {
        pixels_.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0xFF000000);
    }

    void resize(int w, int h) {
        w = std::max(1, w);
        h = std::max(1, h);
        if (w == width_ && h == height_) return;
        width_ = w;
        height_ = h;
        pixels_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0xFF000000);
        damagedRegions_.clear();
        addDamage(Rect{0, 0, width_, height_});
    }

    void clear(RgbaColor c) noexcept {
        uint32_t p = (static_cast<uint32_t>(c.a) << 24) |
                     (static_cast<uint32_t>(c.r) << 16) |
                     (static_cast<uint32_t>(c.g) << 8)  |
                     static_cast<uint32_t>(c.b);
        pixels_.assign(pixels_.size(), p);
        damagedRegions_.clear();
        addDamage(Rect{0, 0, width_, height_});
    }

    [[nodiscard]] int getWidth() const noexcept { return width_; }
    [[nodiscard]] int getHeight() const noexcept { return height_; }
    [[nodiscard]] float getDpiScale() const noexcept { return dpiScale_; }
    void setDpiScale(float dpi) noexcept { dpiScale_ = dpi; }

    [[nodiscard]] uint32_t* data() noexcept { return pixels_.data(); }
    [[nodiscard]] const uint32_t* data() const noexcept { return pixels_.data(); }
    [[nodiscard]] size_t size() const noexcept { return pixels_.size(); }
    [[nodiscard]] const std::vector<uint32_t>& getPixels() const noexcept { return pixels_; }

    [[nodiscard]] inline RgbaColor getPixel(int x, int y) const noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return RgbaColor{};
        uint32_t p = pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
        return RgbaColor{
            static_cast<uint8_t>((p >> 16) & 0xFF),
            static_cast<uint8_t>((p >> 8) & 0xFF),
            static_cast<uint8_t>(p & 0xFF),
            static_cast<uint8_t>((p >> 24) & 0xFF)
        };
    }

    inline void setPixel(int x, int y, RgbaColor c) noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        // Store as ARGB32 (0xAARRGGBB), compatible with Cocoa CGImage, Win32 DIB, and XPutImage
        uint32_t p = (static_cast<uint32_t>(c.a) << 24) |
                     (static_cast<uint32_t>(c.r) << 16) |
                     (static_cast<uint32_t>(c.g) << 8)  |
                     static_cast<uint32_t>(c.b);
        pixels_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] = p;
    }

    inline void blendPixel(int x, int y, RgbaColor src) noexcept {
        if (x < 0 || x >= width_ || y < 0 || y >= height_ || src.a == 0) return;
        if (src.a == 255) {
            setPixel(x, y, src);
            return;
        }

        RgbaColor dst = getPixel(x, y);
        uint32_t sa = src.a;
        uint32_t invA = 255 - sa;

        uint8_t r = static_cast<uint8_t>((src.r * sa + dst.r * invA) / 255);
        uint8_t g = static_cast<uint8_t>((src.g * sa + dst.g * invA) / 255);
        uint8_t b = static_cast<uint8_t>((src.b * sa + dst.b * invA) / 255);
        uint8_t a = static_cast<uint8_t>(std::min(255u, sa + dst.a * invA / 255));

        setPixel(x, y, RgbaColor{r, g, b, a});
    }

    void addDamage(Rect r) {
        if (r.width <= 0 || r.height <= 0) return;
        damagedRegions_.push_back(r);
    }

    [[nodiscard]] const std::vector<Rect>& getDamagedRegions() const noexcept {
        return damagedRegions_;
    }

    void clearDamagedRegions() noexcept {
        damagedRegions_.clear();
    }

private:
    int width_{800};
    int height_{600};
    float dpiScale_{1.0f};
    std::vector<uint32_t> pixels_;
    std::vector<Rect> damagedRegions_;
};

/// 2D Drawing Canvas with anti-aliasing, scissor clipping stack, and glyph rasterization
class Canvas2D {
public:
    explicit Canvas2D(FrameBuffer& fb) : fb_(fb) {
        clipStack_.push_back(Rect{0, 0, fb_.getWidth(), fb_.getHeight()});
        offsetStack_.push_back(Point{0, 0});
    }

    FrameBuffer& getFrameBuffer() noexcept { return fb_; }
    [[nodiscard]] const FrameBuffer& getFrameBuffer() const noexcept { return fb_; }

    // --- Scissor Clipping Stack ---

    void pushClipRect(Rect rect) {
        Point off = getCurrentOffset();
        Rect translated{rect.x + off.x, rect.y + off.y, rect.width, rect.height};
        Rect current = clipStack_.back();

        int x1 = std::max(current.x, translated.x);
        int y1 = std::max(current.y, translated.y);
        int x2 = std::min(current.x + current.width, translated.x + translated.width);
        int y2 = std::min(current.y + current.height, translated.y + translated.height);

        Rect clipped{x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
        clipStack_.push_back(clipped);
    }

    void resetClip() noexcept {
        clipStack_.clear();
        clipStack_.push_back(Rect{0, 0, fb_.getWidth(), fb_.getHeight()});
        offsetStack_.clear();
        offsetStack_.push_back(Point{0, 0});
    }

    void popClipRect() {
        if (clipStack_.size() > 1) {
            clipStack_.pop_back();
        } else if (!clipStack_.empty()) {
            clipStack_[0] = Rect{0, 0, fb_.getWidth(), fb_.getHeight()};
        }
    }

    [[nodiscard]] Rect getCurrentClip() const noexcept {
        return clipStack_.empty() ? Rect{0, 0, fb_.getWidth(), fb_.getHeight()} : clipStack_.back();
    }

    // --- Offset Stack (for scrolling containers) ---

    void pushOffset(Point delta) {
        Point cur = offsetStack_.empty() ? Point{0, 0} : offsetStack_.back();
        offsetStack_.push_back(Point{cur.x + delta.x, cur.y + delta.y});
    }

    void popOffset() {
        if (offsetStack_.size() > 1) {
            offsetStack_.pop_back();
        } else if (!offsetStack_.empty()) {
            offsetStack_[0] = Point{0, 0};
        }
    }

    [[nodiscard]] Point getCurrentOffset() const noexcept {
        return offsetStack_.empty() ? Point{0, 0} : offsetStack_.back();
    }

    // --- Drawing Primitives ---

    void clear(RgbaColor color) {
        resetClip();
        fb_.clear(color);
    }

    void fillRect(Rect rect, RgbaColor color) {
        Point off = getCurrentOffset();
        int rx = rect.x + off.x;
        int ry = rect.y + off.y;
        Rect clip = clipStack_.back();

        int x1 = std::max(clip.x, rx);
        int y1 = std::max(clip.y, ry);
        int x2 = std::min(clip.x + clip.width, rx + rect.width);
        int y2 = std::min(clip.y + clip.height, ry + rect.height);

        for (int y = y1; y < y2; ++y) {
            for (int x = x1; x < x2; ++x) {
                fb_.blendPixel(x, y, color);
            }
        }
    }

    void strokeRect(Rect rect, RgbaColor color, int strokeWidth = 1) {
        if (strokeWidth <= 0 || rect.width <= 0 || rect.height <= 0) return;
        fillRect(Rect{rect.x, rect.y, rect.width, strokeWidth}, color); // top
        fillRect(Rect{rect.x, rect.y + rect.height - strokeWidth, rect.width, strokeWidth}, color); // bottom
        fillRect(Rect{rect.x, rect.y, strokeWidth, rect.height}, color); // left
        fillRect(Rect{rect.x + rect.width - strokeWidth, rect.y, strokeWidth, rect.height}, color); // right
    }

    void drawLine(Point p1, Point p2, RgbaColor color, int thickness = 1) {
        Point off = getCurrentOffset();
        int x0 = p1.x + off.x;
        int y0 = p1.y + off.y;
        int x1 = p2.x + off.x;
        int y1 = p2.y + off.y;

        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        Rect clip = clipStack_.back();

        while (true) {
            for (int ty = -thickness / 2; ty <= thickness / 2; ++ty) {
                for (int tx = -thickness / 2; tx <= thickness / 2; ++tx) {
                    int px = x0 + tx;
                    int py = y0 + ty;
                    if (clip.contains(px, py)) {
                        fb_.blendPixel(px, py, color);
                    }
                }
            }

            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    /// Anti-aliased rounded rectangle fill using signed-distance field (SDF) evaluation
    void fillRoundedRect(Rect rect, int radius, RgbaColor color) {
        if (radius <= 0) {
            fillRect(rect, color);
            return;
        }

        Point off = getCurrentOffset();
        int rx = rect.x + off.x;
        int ry = rect.y + off.y;
        Rect clip = clipStack_.back();

        radius = std::min({radius, rect.width / 2, rect.height / 2});

        int x1 = std::max(clip.x, rx);
        int y1 = std::max(clip.y, ry);
        int x2 = std::min(clip.x + clip.width, rx + rect.width);
        int y2 = std::min(clip.y + clip.height, ry + rect.height);

        float halfW = rect.width / 2.0f;
        float halfH = rect.height / 2.0f;
        float centerX = rx + halfW;
        float centerY = ry + halfH;
        float rF = static_cast<float>(radius);

        for (int y = y1; y < y2; ++y) {
            float py = (y + 0.5f) - centerY;
            float qy = std::abs(py) - halfH + rF;

            for (int x = x1; x < x2; ++x) {
                float px = (x + 0.5f) - centerX;
                float qx = std::abs(px) - halfW + rF;

                if (qx > 0 && qy > 0) {
                    // In corner region: compute distance to corner arc
                    float dist = std::sqrt(qx * qx + qy * qy) - rF;
                    if (dist <= -0.5f) {
                        fb_.blendPixel(x, y, color);
                    } else if (dist < 0.5f) {
                        // Anti-aliased transition edge
                        float coverage = 0.5f - dist;
                        uint8_t alpha = static_cast<uint8_t>(color.a * std::clamp(coverage, 0.0f, 1.0f));
                        RgbaColor aaColor = color;
                        aaColor.a = alpha;
                        fb_.blendPixel(x, y, aaColor);
                    }
                } else {
                    // Inside rect interior
                    fb_.blendPixel(x, y, color);
                }
            }
        }
    }

    /// Anti-aliased rounded rectangle border stroke
    void strokeRoundedRect(Rect rect, int radius, RgbaColor color, int strokeWidth = 1) {
        if (strokeWidth <= 0 || rect.width <= 0 || rect.height <= 0) return;
        if (radius <= 0) {
            strokeRect(rect, color, strokeWidth);
            return;
        }

        Point off = getCurrentOffset();
        int rx = rect.x + off.x;
        int ry = rect.y + off.y;
        Rect clip = clipStack_.back();

        radius = std::min({radius, rect.width / 2, rect.height / 2});

        int x1 = std::max(clip.x, rx);
        int y1 = std::max(clip.y, ry);
        int x2 = std::min(clip.x + clip.width, rx + rect.width);
        int y2 = std::min(clip.y + clip.height, ry + rect.height);

        float halfW = rect.width / 2.0f;
        float halfH = rect.height / 2.0f;
        float centerX = rx + halfW;
        float centerY = ry + halfH;
        float rF = static_cast<float>(radius);
        float halfStroke = strokeWidth / 2.0f;

        for (int y = y1; y < y2; ++y) {
            float py = (y + 0.5f) - centerY;
            float qy = std::abs(py) - halfH + rF;

            for (int x = x1; x < x2; ++x) {
                float px = (x + 0.5f) - centerX;
                float qx = std::abs(px) - halfW + rF;

                float dist = 0.0f;
                if (qx > 0 && qy > 0) {
                    dist = std::sqrt(qx * qx + qy * qy) - rF;
                } else {
                    dist = std::max(qx - rF, qy - rF);
                }

                float edgeDist = std::abs(dist + halfStroke) - halfStroke;
                if (edgeDist <= -0.5f) {
                    fb_.blendPixel(x, y, color);
                } else if (edgeDist < 0.5f) {
                    float coverage = 0.5f - edgeDist;
                    uint8_t alpha = static_cast<uint8_t>(color.a * std::clamp(coverage, 0.0f, 1.0f));
                    RgbaColor aaColor = color;
                    aaColor.a = alpha;
                    fb_.blendPixel(x, y, aaColor);
                }
            }
        }
    }

    // --- Text & Glyph Rasterization ---

    static uint32_t decodeUtf8(std::string_view text, size_t& i) noexcept {
        if (i >= text.size()) return 0;
        uint8_t lead = static_cast<uint8_t>(text[i]);
        if (lead < 0x80) {
            i += 1;
            return lead;
        }
        if ((lead & 0xE0) == 0xC0 && i + 1 < text.size()) {
            uint32_t cp = ((lead & 0x1F) << 6) | (static_cast<uint8_t>(text[i + 1]) & 0x3F);
            i += 2;
            return cp;
        }
        if ((lead & 0xF0) == 0xE0 && i + 2 < text.size()) {
            uint32_t cp = ((lead & 0x0F) << 12) |
                          ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(text[i + 2]) & 0x3F);
            i += 3;
            return cp;
        }
        if ((lead & 0xF8) == 0xF0 && i + 3 < text.size()) {
            uint32_t cp = ((lead & 0x07) << 18) |
                          ((static_cast<uint8_t>(text[i + 1]) & 0x3F) << 12) |
                          ((static_cast<uint8_t>(text[i + 2]) & 0x3F) << 6) |
                          (static_cast<uint8_t>(text[i + 3]) & 0x3F);
            i += 4;
            return cp;
        }
        i += 1;
        return '?';
    }

    static uint8_t mapUnicodeToAscii(uint32_t cp) noexcept {
        if (cp < 128) return static_cast<uint8_t>(cp);
        switch (cp) {
            case 0x2022: // •
            case 0x25CF: // ●
            case 0x25CB: // ○
                return '*';
            case 0x2013: // –
            case 0x2014: // —
                return '-';
            case 0x2018: // ‘
            case 0x2019: // ’
                return '\'';
            case 0x201C: // “
            case 0x201D: // ”
                return '"';
            case 0x26A1: // ⚡
                return '!';
            case 0x1F4D0: // 📐
            case 0x1F4E6: // 📦
                return '#';
            case 0x1F3AF: // 🎯
                return '@';
            default:
                return '?';
        }
    }

    static Size measureText(std::string_view text, int fontSize = 14, const std::string& fontFamily = "") noexcept {
        auto font = FontManager::instance().getFont(fontFamily);
        if (font && font->isValid()) {
            return font->measureText(text, fontSize);
        }

        int charW = std::max(1, (8 * fontSize) / 14);
        int lineH = std::max(1, (20 * fontSize) / 14);
        int charCount = 0;
        size_t i = 0;
        while (i < text.size()) {
            decodeUtf8(text, i);
            charCount++;
        }
        return Size{charCount * charW, lineH};
    }

    static inline uint8_t correctFontAlpha(uint8_t a) noexcept {
        static const auto lut = []() {
            std::array<uint8_t, 256> table{};
            for (int i = 0; i < 256; ++i) {
                float norm = i / 255.0f;
                // Gamma ~1.5 contrast enhancement prevents dark-theme font washing/blur
                float corrected = std::pow(norm, 1.0f / 1.5f);
                table[i] = static_cast<uint8_t>(std::clamp(std::round(corrected * 255.0f), 0.0f, 255.0f));
            }
            return table;
        }();
        return lut[a];
    }

    void drawTrueTypeText(std::string_view text, Point origin, RgbaColor color, int fontSize, FontStyle style, TrueTypeFont& font) {
        Point off = getCurrentOffset();
        int baseX = origin.x + off.x;
        int baseY = origin.y + off.y;
        Rect clip = clipStack_.back();

        int ascent = font.getAscent(fontSize);
        int baselineY = baseY + ascent;
        bool isBold = (style & FontStyle::Bold);
        bool isUnderline = (style & FontStyle::Underline);

        int curX = baseX;
        size_t i = 0;
        uint32_t prevCp = 0;

        while (i < text.size()) {
            uint32_t cp = decodeUtf8(text, i);
            if (prevCp != 0) {
                curX += font.getKernAdvance(prevCp, cp, fontSize);
            }

            const auto* g = font.getGlyph(cp, fontSize);
            if (g) {
                int glyphX = curX + g->xoff;
                int glyphY = baselineY + g->yoff;

                for (int gy = 0; gy < g->height; ++gy) {
                    int py = glyphY + gy;
                    for (int gx = 0; gx < g->width; ++gx) {
                        int px = glyphX + gx;
                        uint8_t rawA = g->bitmap[static_cast<size_t>(gy) * static_cast<size_t>(g->width) + static_cast<size_t>(gx)];
                        if (rawA == 0) continue;
                        uint8_t a = correctFontAlpha(rawA);

                        if (clip.contains(px, py)) {
                            RgbaColor pxCol = color;
                            pxCol.a = static_cast<uint8_t>((static_cast<uint32_t>(color.a) * a) / 255);
                            fb_.blendPixel(px, py, pxCol);
                            if (isBold && clip.contains(px + 1, py)) {
                                fb_.blendPixel(px + 1, py, pxCol);
                            }
                        }
                    }
                }
                curX += g->xadvance;
            } else {
                curX += std::max(1, (8 * fontSize) / 14);
            }
            prevCp = cp;
        }

        if (isUnderline) {
            int lineY = baselineY + 2;
            drawLine(Point{baseX, lineY}, Point{curX, lineY}, color, std::max(1, fontSize / 14));
        }
    }

    void drawText(std::string_view text, Point origin, RgbaColor color, int fontSize = 14, FontStyle style = FontStyle::Normal, const std::string& fontFamily = "") {
        if (text.empty()) return;

        auto font = FontManager::instance().getFont(fontFamily);
        if (font && font->isValid()) {
            drawTrueTypeText(text, origin, color, fontSize, style, *font);
            return;
        }

        Point off = getCurrentOffset();
        int baseX = origin.x + off.x;
        int baseY = origin.y + off.y;
        Rect clip = clipStack_.back();

        int charW = std::max(1, (8 * fontSize) / 14);
        int lineH = std::max(1, (20 * fontSize) / 14);
        int glyphH = std::max(1, (16 * fontSize) / 14);
        int baselineY = baseY + (lineH - glyphH) / 2;

        bool isBold = (style & FontStyle::Bold);
        bool isUnderline = (style & FontStyle::Underline);

        int charIndex = 0;
        size_t i = 0;
        while (i < text.size()) {
            uint32_t cp = decodeUtf8(text, i);
            uint8_t ch = mapUnicodeToAscii(cp);

            int charX = baseX + charIndex * charW;
            charIndex++;

            // Render each row of the 8x8 glyph, scaled to glyphH and charW
            for (int row = 0; row < 8; ++row) {
                uint8_t rowBits = FONT8X8[ch][row];
                if (rowBits == 0) continue;

                int pyStart = baselineY + (row * glyphH) / 8;
                int pyEnd = baselineY + ((row + 1) * glyphH) / 8;

                for (int col = 0; col < 8; ++col) {
                    if ((rowBits & (1 << col)) != 0) {
                        int pxStart = charX + (col * charW) / 8;
                        int pxEnd = charX + ((col + 1) * charW) / 8;

                        for (int py = pyStart; py < pyEnd; ++py) {
                            for (int px = pxStart; px < pxEnd; ++px) {
                                if (clip.contains(px, py)) {
                                    fb_.blendPixel(px, py, color);
                                    if (isBold && clip.contains(px + 1, py)) {
                                        fb_.blendPixel(px + 1, py, color);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Underline decoration
            if (isUnderline) {
                int underlineY = baselineY + glyphH;
                for (int px = charX; px < charX + charW; ++px) {
                    if (clip.contains(px, underlineY)) {
                        fb_.blendPixel(px, underlineY, color);
                    }
                }
            }
        }
    }

private:
    FrameBuffer& fb_;
    std::vector<Rect> clipStack_;
    std::vector<Point> offsetStack_;
};

/// Scene Graph Renderer: walks a UINode tree and draws styled widgets to Canvas2D
class SceneGraphRenderer {
public:
    static void render(const std::shared_ptr<UINode>& root, Canvas2D& canvas, uint32_t focusedNodeId = 0, float dpiScale = 1.0f, bool isFocusVisible = true) {
        if (!root || !root->isVisible()) return;
        renderNode(root, canvas, focusedNodeId, "", dpiScale, isFocusVisible);
    }

private:
    static void renderNode(const std::shared_ptr<UINode>& node, Canvas2D& canvas, uint32_t focusedNodeId, const std::string& inheritedFontFamily, float dpiScale = 1.0f, bool isFocusVisible = true) {
        if (!node || !node->isVisible()) return;

        const auto& style = node->getStyle();
        std::string effectiveFont = !style.fontFamily.empty() ? style.fontFamily : inheritedFontFamily;
        Rect bounds = node->getBounds();

        auto scaleVal = [dpiScale](int val) {
            if (dpiScale <= 1.001f && dpiScale >= 0.999f) return val;
            return static_cast<int>(std::round(val * dpiScale));
        };

        int scaledFontSize = scaleVal(style.fontSize);
        int scaledRadius = scaleVal(style.borderRadius);
        int scaledBorderTop = scaleVal(style.border.top);
        Edges scaledPadding{scaleVal(style.padding.top), scaleVal(style.padding.right), scaleVal(style.padding.bottom), scaleVal(style.padding.left)};

        // 1. Render Background & Borders
        RgbaColor bg = style.backgroundColorRgba.value_or(
            style.backgroundColor != Color::Default ? RgbaColor::fromThemeColor(style.backgroundColor, true)
                                                    : RgbaColor{0, 0, 0, 0}
        );
        RgbaColor border = style.borderColorRgba.value_or(
            style.borderColor != Color::Default ? RgbaColor::fromThemeColor(style.borderColor)
                                                : RgbaColor{0, 0, 0, 0}
        );

        if (bg.a > 0) {
            if (scaledRadius > 0) {
                canvas.fillRoundedRect(bounds, scaledRadius, bg);
            } else {
                canvas.fillRect(bounds, bg);
            }
        }

        if (border.a > 0 && scaledBorderTop > 0) {
            if (scaledRadius > 0) {
                canvas.strokeRoundedRect(bounds, scaledRadius, border, scaledBorderTop);
            } else {
                canvas.strokeRect(bounds, border, scaledBorderTop);
            }
        }

        // 2. Node Specific Rendering
        switch (node->getKind()) {
            case NodeKind::Text: {
                auto textNode = std::static_pointer_cast<TextNode>(node);
                RgbaColor textCol = style.textColorRgba.value_or(
                    style.color != Color::Default ? RgbaColor::fromThemeColor(style.color)
                                                  : RgbaColor{205, 214, 244, 255}
                );
                Point textPos{bounds.x + scaledPadding.left, bounds.y + scaledPadding.top};
                canvas.drawText(textNode->getContent(), textPos, textCol, scaledFontSize, style.fontStyle, effectiveFont);
                break;
            }

            case NodeKind::Button: {
                auto btnNode = std::static_pointer_cast<ButtonNode>(node);
                bool isFocused = (node->getId() == focusedNodeId && focusedNodeId != 0);
                bool showFocusRing = isFocused && isFocusVisible;

                RgbaColor btnBg = style.backgroundColorRgba.value_or(
                    showFocusRing ? RgbaColor{69, 71, 90, 255} : RgbaColor{49, 50, 68, 255}
                );
                int radius = scaledRadius > 0 ? scaledRadius : scaleVal(6);
                canvas.fillRoundedRect(bounds, radius, btnBg);

                RgbaColor btnBorder = showFocusRing ? RgbaColor{137, 180, 250, 255}
                                                    : style.borderColorRgba.value_or(RgbaColor{88, 91, 112, 255});
                int bWidth = showFocusRing ? scaleVal(2) : (scaledBorderTop > 0 ? scaledBorderTop : scaleVal(1));
                canvas.strokeRoundedRect(bounds, radius, btnBorder, bWidth);

                RgbaColor btnTextCol = style.textColorRgba.value_or(RgbaColor{255, 255, 255, 255});
                Size textSize = Canvas2D::measureText(btnNode->getLabel(), scaledFontSize, effectiveFont);
                Point textPos{
                    bounds.x + std::max(0, (bounds.width - textSize.width) / 2),
                    bounds.y + std::max(0, (bounds.height - textSize.height) / 2)
                };
                canvas.drawText(btnNode->getLabel(), textPos, btnTextCol, scaledFontSize, FontStyle::Bold, effectiveFont);
                break;
            }

            case NodeKind::Input: {
                auto inputNode = std::static_pointer_cast<InputNode>(node);
                bool isFocused = (node->getId() == focusedNodeId && focusedNodeId != 0);

                RgbaColor inputBg = style.backgroundColorRgba.value_or(RgbaColor{24, 24, 37, 255});
                int radius = scaledRadius > 0 ? scaledRadius : scaleVal(4);
                canvas.fillRoundedRect(bounds, radius, inputBg);

                RgbaColor inputBorder = isFocused ? RgbaColor{137, 180, 250, 255}
                                                  : style.borderColorRgba.value_or(RgbaColor{69, 71, 90, 255});
                canvas.strokeRoundedRect(bounds, radius, inputBorder, isFocused ? scaleVal(2) : scaleVal(1));

                RgbaColor textCol = style.textColorRgba.value_or(RgbaColor{205, 214, 244, 255});
                Size sampleSize = Canvas2D::measureText("Ag", scaledFontSize, effectiveFont);
                int textH = sampleSize.height > 0 ? sampleSize.height : scaledFontSize;
                int textY = bounds.y + std::max(0, (bounds.height - textH) / 2);
                Point textPos{bounds.x + scaleVal(10), textY};

                // Selection highlight & caret vertical positioning
                Size totalTextSize = Canvas2D::measureText(inputNode->getValue(), scaledFontSize, effectiveFont);
                int caretH = textH + scaleVal(4);
                int caretY = bounds.y + std::max(0, (bounds.height - caretH) / 2);

                if (isFocused && inputNode->hasSelection()) {
                    Rect selRect{textPos.x - scaleVal(1), caretY, totalTextSize.width + scaleVal(4), caretH};
                    canvas.fillRect(selRect, RgbaColor{88, 91, 112, 180});
                }

                canvas.drawText(inputNode->getValue(), textPos, textCol, scaledFontSize, FontStyle::Normal, effectiveFont);

                // Cursor caret when focused
                if (isFocused && !inputNode->hasSelection()) {
                    int cursorIndex = std::clamp(inputNode->getCursorPos(), 0, static_cast<int>(inputNode->getValue().size()));
                    std::string_view prefix = std::string_view(inputNode->getValue()).substr(0, cursorIndex);
                    Size prefixSize = Canvas2D::measureText(prefix, scaledFontSize, effectiveFont);
                    Point caretStart{textPos.x + prefixSize.width, caretY};
                    Point caretEnd{textPos.x + prefixSize.width, caretY + caretH};
                    canvas.drawLine(caretStart, caretEnd, RgbaColor{245, 224, 220, 255}, scaleVal(2));
                }
                break;
            }

            case NodeKind::Scroll: {
                auto scrollNode = std::static_pointer_cast<ScrollNode>(node);
                canvas.pushClipRect(bounds);
                canvas.pushOffset(Point{0, -scrollNode->getScrollOffset()});

                for (const auto& child : node->getChildren()) {
                    renderNode(child, canvas, focusedNodeId, effectiveFont, dpiScale, isFocusVisible);
                }

                canvas.popOffset();
                canvas.popClipRect();

                // Draw vertical scrollbar track & thumb if content overflows
                int contentH = scrollNode->getContentHeight();
                int trackW = scaleVal(4);
                if (contentH > bounds.height && bounds.height > scaleVal(20)) {
                    int trackX = bounds.x + bounds.width - trackW - scaleVal(2);
                    int trackY = bounds.y + scaleVal(2);
                    int trackH = bounds.height - scaleVal(4);

                    // Scrollbar track
                    canvas.fillRoundedRect(Rect{trackX, trackY, trackW, trackH}, scaleVal(2), RgbaColor{49, 50, 68, 180});

                    // Scrollbar thumb
                    int thumbH = std::max(scaleVal(16), (trackH * bounds.height) / contentH);
                    int maxScroll = contentH - bounds.height;
                    int thumbY = trackY + (maxScroll > 0 ? (scrollNode->getScrollOffset() * (trackH - thumbH)) / maxScroll : 0);
                    canvas.fillRoundedRect(Rect{trackX, thumbY, trackW, thumbH}, scaleVal(2), RgbaColor{147, 153, 178, 220});
                }
                return; // Children already rendered inside scroll clipping
            }

            default:
                break;
        }

        // 3. Render Children for general nodes
        for (const auto& child : node->getChildren()) {
            renderNode(child, canvas, focusedNodeId, effectiveFont, dpiScale, isFocusVisible);
        }
    }
};

} // namespace keet::ui

