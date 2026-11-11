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

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <keet/ui/geometry.hpp>
#include <keet/ui/utf8.hpp>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace keet::ui {

struct RasterGlyph {
    int width{0};
    int height{0};
    int xoff{0};
    int yoff{0};
    int xadvance{0};
    std::vector<uint8_t> bitmap; // 8-bit alpha (0..255)
};

class TrueTypeFont {
public:
    explicit TrueTypeFont(std::vector<uint8_t> buffer)
        : fontBuffer_(std::move(buffer)), valid_(false) {
        init();
    }

    static std::shared_ptr<TrueTypeFont> loadFromFile(const std::string& path) {
        std::string resolved = path;
        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            if (std::filesystem::exists("../" + path, ec)) {
                resolved = "../" + path;
            }
        }
        std::ifstream file(resolved, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return nullptr;
        std::streamsize size = file.tellg();
        if (size <= 0) return nullptr;
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return nullptr;
        auto font = std::make_shared<TrueTypeFont>(std::move(buffer));
        return font->isValid() ? font : nullptr;
    }

    [[nodiscard]] bool isValid() const noexcept { return valid_; }

    const RasterGlyph* getGlyph(uint32_t codepoint, int fontSize) {
        if (!valid_ || fontSize <= 0) return nullptr;
        uint64_t key = (static_cast<uint64_t>(fontSize) << 32) | codepoint;
        auto it = glyphCache_.find(key);
        if (it != glyphCache_.end()) {
            return &it->second;
        }

        int glyphIndex = stbtt_FindGlyphIndex(&info_, static_cast<int>(codepoint));
        if (glyphIndex <= 0) {
            if (codepoint == 0x2713 || codepoint == 0x2714) {
                return synthesizeCheckmarkGlyph(key, fontSize, codepoint == 0x2714);
            }
            if (codepoint == 0x2715 || codepoint == 0x2716) {
                return synthesizeCrossGlyph(key, fontSize, codepoint == 0x2716);
            }
            return nullptr;
        }

        float scale = stbtt_ScaleForPixelHeight(&info_, static_cast<float>(fontSize));
        int advWidth = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&info_, glyphIndex, &advWidth, &lsb);

        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* bmp = stbtt_GetGlyphBitmap(
            &info_, scale, scale, glyphIndex, &w, &h, &xoff, &yoff
        );

        RasterGlyph glyph;
        glyph.width = w;
        glyph.height = h;
        glyph.xoff = xoff;
        glyph.yoff = yoff;
        glyph.xadvance = static_cast<int>(std::round(advWidth * scale));

        if (bmp && w > 0 && h > 0) {
            glyph.bitmap.assign(bmp, bmp + (w * h));
            stbtt_FreeBitmap(bmp, nullptr);
        }

        auto inserted = glyphCache_.emplace(key, std::move(glyph));
        return &inserted.first->second;
    }

    int getKernAdvance(uint32_t cp1, uint32_t cp2, int fontSize) {
        if (!valid_ || fontSize <= 0) return 0;
        float scale = stbtt_ScaleForPixelHeight(&info_, static_cast<float>(fontSize));
        int g1 = stbtt_FindGlyphIndex(&info_, static_cast<int>(cp1));
        int g2 = stbtt_FindGlyphIndex(&info_, static_cast<int>(cp2));
        if (g1 <= 0 || g2 <= 0) return 0;
        int kern = stbtt_GetGlyphKernAdvance(&info_, g1, g2);
        return static_cast<int>(std::round(kern * scale));
    }

    Size measureText(std::string_view text, int fontSize) {
        if (!valid_ || text.empty() || fontSize <= 0) {
            return Size{0, fontSize};
        }

        float scale = stbtt_ScaleForPixelHeight(&info_, static_cast<float>(fontSize));
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info_, &ascent, &descent, &lineGap);
        int lineH = static_cast<int>(std::round((ascent - descent + lineGap) * scale));
        if (lineH <= 0) lineH = static_cast<int>(fontSize * 1.35f);

        int totalWidth = 0;
        size_t i = 0;
        uint32_t prevCp = 0;

        while (i < text.size()) {
            uint32_t cp = decodeUtf8(text, i);
            if (prevCp != 0) {
                totalWidth += getKernAdvance(prevCp, cp, fontSize);
            }
            const auto* g = getGlyph(cp, fontSize);
            if (g) {
                totalWidth += g->xadvance;
            } else {
                totalWidth += std::max(1, (8 * fontSize) / 14);
            }
            prevCp = cp;
        }

        return Size{totalWidth, lineH};
    }

    [[nodiscard]] int getAscent(int fontSize) {
        if (!valid_) return fontSize;
        float scale = stbtt_ScaleForPixelHeight(&info_, static_cast<float>(fontSize));
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info_, &ascent, &descent, &lineGap);
        return static_cast<int>(std::round(ascent * scale));
    }

private:
    const RasterGlyph* synthesizeCheckmarkGlyph(uint64_t key, int fontSize, bool heavy = false) {
        int W = std::max(8, fontSize);
        int H = W;
        RasterGlyph glyph;
        glyph.width = W;
        glyph.height = H;
        glyph.xoff = 0;
        int ascent = getAscent(fontSize);
        if (ascent <= 0) ascent = static_cast<int>(fontSize * 0.8f);
        glyph.yoff = -ascent;
        glyph.xadvance = W;
        glyph.bitmap.resize(static_cast<size_t>(W * H), 0);

        float stroke = heavy ? std::max(2.2f, W * 0.22f) : std::max(1.6f, W * 0.15f);
        float r = stroke * 0.5f;

        // Points for the checkmark:
        // P1: start of short leg (left)
        // P2: vertex (bottom)
        // P3: end of long leg (top right)
        float x1 = 0.12f * W;
        float y1 = 0.50f * H;
        float x2 = 0.38f * W;
        float y2 = 0.80f * H;
        float x3 = 0.88f * W;
        float y3 = 0.18f * H;

        auto distToSeg = [](float px, float py, float ax, float ay, float bx, float by) -> float {
            float dx = bx - ax;
            float dy = by - ay;
            float lenSq = dx * dx + dy * dy;
            if (lenSq < 1e-4f) {
                float ex = px - ax, ey = py - ay;
                return std::sqrt(ex * ex + ey * ey);
            }
            float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
            t = std::clamp(t, 0.0f, 1.0f);
            float projX = ax + t * dx;
            float projY = ay + t * dy;
            float ex = px - projX, ey = py - projY;
            return std::sqrt(ex * ex + ey * ey);
        };

        for (int y = 0; y < H; ++y) {
            float py = y + 0.5f;
            for (int x = 0; x < W; ++x) {
                float px = x + 0.5f;
                float d1 = distToSeg(px, py, x1, y1, x2, y2);
                float d2 = distToSeg(px, py, x2, y2, x3, y3);
                float minDist = std::min(d1, d2);
                float alpha = std::clamp(r + 0.5f - minDist, 0.0f, 1.0f);
                glyph.bitmap[static_cast<size_t>(y * W + x)] = static_cast<uint8_t>(std::round(alpha * 255.0f));
            }
        }

        auto inserted = glyphCache_.emplace(key, std::move(glyph));
        return &inserted.first->second;
    }

    const RasterGlyph* synthesizeCrossGlyph(uint64_t key, int fontSize, bool heavy = false) {
        int W = std::max(8, fontSize);
        int H = W;
        RasterGlyph glyph;
        glyph.width = W;
        glyph.height = H;
        glyph.xoff = 0;
        int ascent = getAscent(fontSize);
        if (ascent <= 0) ascent = static_cast<int>(fontSize * 0.8f);
        glyph.yoff = -ascent;
        glyph.xadvance = W;
        glyph.bitmap.resize(static_cast<size_t>(W * H), 0);

        float stroke = heavy ? std::max(2.2f, W * 0.22f) : std::max(1.6f, W * 0.15f);
        float r = stroke * 0.5f;

        float x1 = 0.20f * W, y1 = 0.20f * H;
        float x2 = 0.80f * W, y2 = 0.80f * H;
        float x3 = 0.80f * W, y3 = 0.20f * H;
        float x4 = 0.20f * W, y4 = 0.80f * H;

        auto distToSeg = [](float px, float py, float ax, float ay, float bx, float by) -> float {
            float dx = bx - ax;
            float dy = by - ay;
            float lenSq = dx * dx + dy * dy;
            if (lenSq < 1e-4f) {
                float ex = px - ax, ey = py - ay;
                return std::sqrt(ex * ex + ey * ey);
            }
            float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
            t = std::clamp(t, 0.0f, 1.0f);
            float projX = ax + t * dx;
            float projY = ay + t * dy;
            float ex = px - projX, ey = py - projY;
            return std::sqrt(ex * ex + ey * ey);
        };

        for (int y = 0; y < H; ++y) {
            float py = y + 0.5f;
            for (int x = 0; x < W; ++x) {
                float px = x + 0.5f;
                float d1 = distToSeg(px, py, x1, y1, x2, y2);
                float d2 = distToSeg(px, py, x3, y3, x4, y4);
                float minDist = std::min(d1, d2);
                float alpha = std::clamp(r + 0.5f - minDist, 0.0f, 1.0f);
                glyph.bitmap[static_cast<size_t>(y * W + x)] = static_cast<uint8_t>(std::round(alpha * 255.0f));
            }
        }

        auto inserted = glyphCache_.emplace(key, std::move(glyph));
        return &inserted.first->second;
    }

    void init() {
        if (fontBuffer_.empty()) return;
        int offset = stbtt_GetFontOffsetForIndex(fontBuffer_.data(), 0);
        if (offset < 0) return;
        if (stbtt_InitFont(&info_, fontBuffer_.data(), offset) != 0) {
            valid_ = true;
        }
    }

    std::vector<uint8_t> fontBuffer_;
    stbtt_fontinfo info_{};
    bool valid_{false};
    std::unordered_map<uint64_t, RasterGlyph> glyphCache_;
};

class FontManager {
public:
    static FontManager& instance() {
        static FontManager inst;
        return inst;
    }

    bool registerFont(const std::string& family, const std::string& path, bool setAsDefault = true) {
        auto font = TrueTypeFont::loadFromFile(path);
        if (!font) return false;
        fonts_[family] = font;
        if (!defaultFont_ || setAsDefault) {
            defaultFont_ = font;
        }
        return true;
    }

    bool registerFontFromMemory(const std::string& family, std::vector<uint8_t> data, bool setAsDefault = true) {
        auto font = std::make_shared<TrueTypeFont>(std::move(data));
        if (!font->isValid()) return false;
        fonts_[family] = font;
        if (!defaultFont_ || setAsDefault) {
            defaultFont_ = font;
        }
        return true;
    }

    void setDefaultFont(std::shared_ptr<TrueTypeFont> font) {
        defaultFont_ = std::move(font);
    }

    [[nodiscard]] std::shared_ptr<TrueTypeFont> getFont(const std::string& family = "") const {
        if (!family.empty()) {
            auto it = fonts_.find(family);
            if (it != fonts_.end()) return it->second;
        }
        return defaultFont_;
    }

    [[nodiscard]] std::shared_ptr<TrueTypeFont> getDefaultFont() const {
        return defaultFont_;
    }

    void initSystemDefaults() {
        if (defaultFont_) return;

        // Try standard modern system fonts
        const std::vector<std::string> candidates = {
            "assets/Inter.ttf",
            "../assets/Inter.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "C:\\Windows\\Fonts\\arial.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
        };

        for (const auto& path : candidates) {
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                if (registerFont("Default", path)) {
                    break;
                }
            }
        }
    }

private:
    FontManager() {
        initSystemDefaults();
    }

    std::unordered_map<std::string, std::shared_ptr<TrueTypeFont>> fonts_;
    std::shared_ptr<TrueTypeFont> defaultFont_{nullptr};
};

} // namespace keet::ui
