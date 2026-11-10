#pragma once
#include <cstdint>
#include <string_view>

namespace keet::ui {

inline uint32_t decodeUtf8(std::string_view text, size_t& i) noexcept {
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

inline uint8_t mapUnicodeToAscii(uint32_t cp) noexcept {
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

} // namespace keet::ui

