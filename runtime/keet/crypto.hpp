#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#if defined(_MSC_VER)
#pragma comment(lib, "bcrypt.lib")
#endif
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace keet::crypto {

// ============================================================================
// Phase 1.1: Data Encodings (Hexadecimal, Base64, Base64URL)
// ============================================================================

namespace detail {

inline constexpr char HEX_DIGITS_LOWER[] = "0123456789abcdef";

inline int hexCharToInt(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline constexpr char B64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

inline constexpr char B64URL_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

inline int b64CharToInt(char c, bool url_safe) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (!url_safe) {
        if (c == '+') return 62;
        if (c == '/') return 63;
    } else {
        if (c == '-') return 62;
        if (c == '_') return 63;
    }
    return -1;
}

} // namespace detail

/// Converts a binary or text string into a lowercase hexadecimal string.
inline std::string toHex(const std::string& data) {
    std::string out;
    out.reserve(data.size() * 2);
    for (unsigned char c : data) {
        out.push_back(detail::HEX_DIGITS_LOWER[(c >> 4) & 0x0F]);
        out.push_back(detail::HEX_DIGITS_LOWER[c & 0x0F]);
    }
    return out;
}

/// Decodes a hexadecimal string into raw bytes. Throws keet::error on failure.
inline std::string fromHex(const std::string& hexStr) {
    if (hexStr.size() % 2 != 0) {
        throw keet::error("CryptoError", "Odd-length hexadecimal input string");
    }
    std::string out;
    out.reserve(hexStr.size() / 2);
    for (size_t i = 0; i < hexStr.size(); i += 2) {
        int hi = detail::hexCharToInt(hexStr[i]);
        if (hi < 0) {
            throw keet::error("CryptoError", "Invalid hexadecimal character '" + std::string(1, hexStr[i]) +
                                                 "' at position " + std::to_string(i));
        }
        int lo = detail::hexCharToInt(hexStr[i + 1]);
        if (lo < 0) {
            throw keet::error("CryptoError", "Invalid hexadecimal character '" + std::string(1, hexStr[i + 1]) +
                                                 "' at position " + std::to_string(i + 1));
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return out;
}

/// Encodes raw string data into standard RFC 4648 Base64 string with '=' padding.
inline std::string toBase64(const std::string& data) {
    std::string out;
    size_t len = data.size();
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        size_t rem = len - i;
        uint32_t octet_a = static_cast<unsigned char>(data[i]);
        uint32_t octet_b = (rem > 1) ? static_cast<unsigned char>(data[i + 1]) : 0;
        uint32_t octet_c = (rem > 2) ? static_cast<unsigned char>(data[i + 2]) : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(detail::B64_CHARS[(triple >> 18) & 0x3F]);
        out.push_back(detail::B64_CHARS[(triple >> 12) & 0x3F]);
        out.push_back((rem > 1) ? detail::B64_CHARS[(triple >> 6) & 0x3F] : '=');
        out.push_back((rem > 2) ? detail::B64_CHARS[triple & 0x3F] : '=');
    }
    return out;
}

/// Decodes standard RFC 4648 Base64 string. Throws keet::error on invalid syntax.
inline std::string fromBase64(const std::string& b64) {
    // Strip trailing padding '='
    size_t pad = 0;
    size_t len = b64.size();
    while (len > 0 && b64[len - 1] == '=') {
        pad++;
        len--;
    }
    if (pad > 2) {
        throw keet::error("CryptoError", "Invalid Base64 padding: too many '=' characters");
    }
    if ((b64.size() % 4 != 0 && b64.size() != 0) || (len % 4 == 1)) {
        throw keet::error("CryptoError", "Invalid Base64 input length");
    }

    std::string out;
    out.reserve((b64.size() * 3) / 4);

    for (size_t i = 0; i < len; i += 4) {
        uint32_t val = 0;
        int count = 0;
        for (size_t j = 0; j < 4 && (i + j) < len; ++j) {
            int d = detail::b64CharToInt(b64[i + j], false);
            if (d < 0) {
                throw keet::error("CryptoError", "Invalid Base64 character '" + std::string(1, b64[i + j]) +
                                                     "' at position " + std::to_string(i + j));
            }
            val = (val << 6) | static_cast<uint32_t>(d);
            count++;
        }

        if (count == 4) {
            out.push_back(static_cast<char>((val >> 16) & 0xFF));
            out.push_back(static_cast<char>((val >> 8) & 0xFF));
            out.push_back(static_cast<char>(val & 0xFF));
        } else if (count == 3) {
            val <<= 6;
            out.push_back(static_cast<char>((val >> 16) & 0xFF));
            out.push_back(static_cast<char>((val >> 8) & 0xFF));
        } else if (count == 2) {
            val <<= 12;
            out.push_back(static_cast<char>((val >> 16) & 0xFF));
        }
    }
    return out;
}

/// Encodes raw string data into RFC 4648 URL-safe Base64 without '=' padding.
inline std::string toBase64Url(const std::string& data) {
    std::string b64 = toBase64(data);
    std::string out;
    out.reserve(b64.size());
    for (char c : b64) {
        if (c == '+') {
            out.push_back('-');
        } else if (c == '/') {
            out.push_back('_');
        } else if (c != '=') {
            out.push_back(c);
        }
    }
    return out;
}

/// Decodes RFC 4648 URL-safe Base64 string (handles with or without padding).
inline std::string fromBase64Url(const std::string& b64url) {
    std::string b64;
    b64.reserve(b64url.size() + 3);
    for (char c : b64url) {
        if (c == '-') {
            b64.push_back('+');
        } else if (c == '_') {
            b64.push_back('/');
        } else if (c != '=') {
            b64.push_back(c);
        }
    }
    while (b64.size() % 4 != 0) {
        b64.push_back('=');
    }
    return fromBase64(b64);
}

// ============================================================================
// Phase 1.2: Cryptographically Secure Pseudo-Random Number Generation (CSPRNG)
// ============================================================================

namespace detail {

inline void getSystemEntropy(void* dest, size_t length) {
    if (length == 0) return;

#if defined(_WIN32)
    NTSTATUS status = BCryptGenRandom(nullptr, static_cast<PUCHAR>(dest), static_cast<ULONG>(length),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        throw keet::error("CryptoError", "BCryptGenRandom failed to acquire secure entropy");
    }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    arc4random_buf(dest, length);
#else
    // Linux / POSIX getrandom with /dev/urandom fallback
    unsigned char* ptr = static_cast<unsigned char*>(dest);
    size_t remaining = length;
    while (remaining > 0) {
        ssize_t ret = getrandom(ptr, remaining, 0);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            // Fallback to /dev/urandom on older kernels or ENOSYS
            int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                throw keet::error("CryptoError", "Failed to open /dev/urandom for secure entropy");
            }
            while (remaining > 0) {
                ssize_t read_bytes = read(fd, ptr, remaining);
                if (read_bytes < 0) {
                    if (errno == EINTR) continue;
                    close(fd);
                    throw keet::error("CryptoError", "Failed to read from /dev/urandom");
                }
                remaining -= static_cast<size_t>(read_bytes);
                ptr += read_bytes;
            }
            close(fd);
            return;
        }
        remaining -= static_cast<size_t>(ret);
        ptr += ret;
    }
#endif
}

} // namespace detail

/// Generates a binary string of cryptographically secure random bytes of specified length.
inline std::string randomBytes(int64_t length) {
    if (length < 0) {
        throw keet::error("CryptoError", "randomBytes length cannot be negative");
    }
    if (length == 0) {
        return "";
    }
    std::string out(static_cast<size_t>(length), '\0');
    detail::getSystemEntropy(out.data(), out.size());
    return out;
}

/// Generates a random hexadecimal string representing `length` random bytes (length * 2 characters).
inline std::string randomHex(int64_t length) {
    if (length < 0) {
        throw keet::error("CryptoError", "randomHex length cannot be negative");
    }
    return toHex(randomBytes(length));
}

/// Generates a cryptographically uniform random integer in the closed interval [min, max].
inline int64_t randomInt(int64_t min, int64_t max) {
    if (min > max) {
        throw keet::error("CryptoError", "randomInt: minimum value (" + std::to_string(min) +
                                             ") cannot exceed maximum value (" + std::to_string(max) + ")");
    }
    if (min == max) {
        return min;
    }

    uint64_t range = static_cast<uint64_t>(max - min) + 1;
    if (range == 0) {
        // Full uint64 range overflow
        uint64_t val = 0;
        detail::getSystemEntropy(&val, sizeof(val));
        return static_cast<int64_t>(val);
    }

    // Unbiased rejection sampling (Debruijn / Lemire technique)
    uint64_t limit = (UINT64_MAX / range) * range;
    uint64_t val = 0;
    do {
        detail::getSystemEntropy(&val, sizeof(val));
    } while (val >= limit);

    return min + static_cast<int64_t>(val % range);
}

/// Generates a cryptographically random RFC 4122 version 4 UUID string.
inline std::string randomUuid() {
    uint8_t bytes[16];
    detail::getSystemEntropy(bytes, sizeof(bytes));

    // Set version to 4 (0100 in high 4 bits of octet 6)
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    // Set variant to RFC 4122 (10xx in high 2 bits of octet 8)
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3],
                  bytes[4], bytes[5],
                  bytes[6], bytes[7],
                  bytes[8], bytes[9],
                  bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf, 36);
}

// ============================================================================
// Phase 1.3: Side-Channel Hardening
// ============================================================================

/// Performs a constant-time byte comparison between two strings to prevent timing attacks.
inline BoolByte timingSafeEqual(const std::string& a, const std::string& b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    volatile const unsigned char* pa = reinterpret_cast<volatile const unsigned char*>(a.data());
    volatile const unsigned char* pb = reinterpret_cast<volatile const unsigned char*>(b.data());
    unsigned char result = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        result |= (pa[i] ^ pb[i]);
    }
    return result == 0;
}

} // namespace keet::crypto
