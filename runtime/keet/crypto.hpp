#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
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

// ============================================================================
// Phase 2.1: Modern Cryptographic Hashes (SHA-256, SHA-512, SHA-3, BLAKE3)
// ============================================================================

namespace detail {

// ----------------------------------------------------------------------------
// SHA-256 Implementation (FIPS 180-4)
// ----------------------------------------------------------------------------
struct Sha256Core {
    uint32_t state[8]{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t buffer[64]{0};
    size_t buffer_len{0};
    uint64_t total_bits{0};

    static inline constexpr uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    void processBlock(const uint8_t* block) {
        uint32_t W[64];
        for (int t = 0; t < 16; ++t) {
            W[t] = (static_cast<uint32_t>(block[t * 4]) << 24) |
                   (static_cast<uint32_t>(block[t * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[t * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(block[t * 4 + 3]));
        }
        for (int t = 16; t < 64; ++t) {
            uint32_t s0 = std::rotr(W[t - 15], 7) ^ std::rotr(W[t - 15], 18) ^ (W[t - 15] >> 3);
            uint32_t s1 = std::rotr(W[t - 2], 17) ^ std::rotr(W[t - 2], 19) ^ (W[t - 2] >> 10);
            W[t] = W[t - 16] + s0 + W[t - 7] + s1;
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int t = 0; t < 64; ++t) {
            uint32_t S1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + K[t] + W[t];
            uint32_t S0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const void* data, size_t length) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        total_bits += static_cast<uint64_t>(length) * 8;
        while (length > 0) {
            size_t to_copy = std::min(length, size_t{64} - buffer_len);
            std::memcpy(buffer + buffer_len, ptr, to_copy);
            buffer_len += to_copy;
            ptr += to_copy;
            length -= to_copy;
            if (buffer_len == 64) {
                processBlock(buffer);
                buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        buffer[buffer_len++] = 0x80;
        if (buffer_len > 56) {
            std::memset(buffer + buffer_len, 0, 64 - buffer_len);
            processBlock(buffer);
            buffer_len = 0;
        }
        std::memset(buffer + buffer_len, 0, 56 - buffer_len);
        for (int i = 0; i < 8; ++i) {
            buffer[56 + i] = static_cast<uint8_t>((total_bits >> (56 - i * 8)) & 0xFF);
        }
        processBlock(buffer);

        std::string digest(32, '\0');
        for (int i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<char>((state[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<char>((state[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<char>((state[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<char>(state[i] & 0xFF);
        }
        return digest;
    }

    void reset() {
        state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
        state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
        buffer_len = 0;
        total_bits = 0;
    }
};

// ----------------------------------------------------------------------------
// SHA-512 Implementation (FIPS 180-4)
// ----------------------------------------------------------------------------
struct Sha512Core {
    uint64_t state[8]{
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };
    uint8_t buffer[128]{0};
    size_t buffer_len{0};
    uint64_t total_bits{0};

    static inline constexpr uint64_t K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
        0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
        0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
        0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
        0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
        0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
        0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
        0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
        0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
    };

    void processBlock(const uint8_t* block) {
        uint64_t W[80];
        for (int t = 0; t < 16; ++t) {
            W[t] = (static_cast<uint64_t>(block[t * 8]) << 56) |
                   (static_cast<uint64_t>(block[t * 8 + 1]) << 48) |
                   (static_cast<uint64_t>(block[t * 8 + 2]) << 40) |
                   (static_cast<uint64_t>(block[t * 8 + 3]) << 32) |
                   (static_cast<uint64_t>(block[t * 8 + 4]) << 24) |
                   (static_cast<uint64_t>(block[t * 8 + 5]) << 16) |
                   (static_cast<uint64_t>(block[t * 8 + 6]) << 8) |
                   (static_cast<uint64_t>(block[t * 8 + 7]));
        }
        for (int t = 16; t < 80; ++t) {
            uint64_t s0 = std::rotr(W[t - 15], 1) ^ std::rotr(W[t - 15], 8) ^ (W[t - 15] >> 7);
            uint64_t s1 = std::rotr(W[t - 2], 19) ^ std::rotr(W[t - 2], 61) ^ (W[t - 2] >> 6);
            W[t] = W[t - 16] + s0 + W[t - 7] + s1;
        }

        uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int t = 0; t < 80; ++t) {
            uint64_t S1 = std::rotr(e, 14) ^ std::rotr(e, 18) ^ std::rotr(e, 41);
            uint64_t ch = (e & f) ^ ((~e) & g);
            uint64_t temp1 = h + S1 + ch + K[t] + W[t];
            uint64_t S0 = std::rotr(a, 28) ^ std::rotr(a, 34) ^ std::rotr(a, 39);
            uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint64_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(const void* data, size_t length) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        total_bits += static_cast<uint64_t>(length) * 8;
        while (length > 0) {
            size_t to_copy = std::min(length, size_t{128} - buffer_len);
            std::memcpy(buffer + buffer_len, ptr, to_copy);
            buffer_len += to_copy;
            ptr += to_copy;
            length -= to_copy;
            if (buffer_len == 128) {
                processBlock(buffer);
                buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        buffer[buffer_len++] = 0x80;
        if (buffer_len > 112) {
            std::memset(buffer + buffer_len, 0, 128 - buffer_len);
            processBlock(buffer);
            buffer_len = 0;
        }
        std::memset(buffer + buffer_len, 0, 112 - buffer_len);
        // High 64 bits of bit length are 0 for sizes < 2^64 bits
        std::memset(buffer + 112, 0, 8);
        for (int i = 0; i < 8; ++i) {
            buffer[120 + i] = static_cast<uint8_t>((total_bits >> (56 - i * 8)) & 0xFF);
        }
        processBlock(buffer);

        std::string digest(64, '\0');
        for (int i = 0; i < 8; ++i) {
            for (int b = 0; b < 8; ++b) {
                digest[i * 8 + b] = static_cast<char>((state[i] >> (56 - b * 8)) & 0xFF);
            }
        }
        return digest;
    }

    void reset() {
        state[0] = 0x6a09e667f3bcc908ULL; state[1] = 0xbb67ae8584caa73bULL;
        state[2] = 0x3c6ef372fe94f82bULL; state[3] = 0xa54ff53a5f1d36f1ULL;
        state[4] = 0x510e527fade682d1ULL; state[5] = 0x9b05688c2b3e6c1fULL;
        state[6] = 0x1f83d9abfb41bd6bULL; state[7] = 0x5be0cd19137e2179ULL;
        buffer_len = 0;
        total_bits = 0;
    }
};

// ----------------------------------------------------------------------------
// SHA-3 / Keccak Implementation (FIPS 202)
// ----------------------------------------------------------------------------
inline constexpr uint64_t KECCAK_RC[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

inline constexpr int KECCAK_ROTC[5][5] = {
    { 0, 36,  3, 41, 18},
    { 1, 44, 10, 45,  2},
    {62,  6, 43, 15, 61},
    {28, 55, 25, 21, 56},
    {27, 20, 39,  8, 14}
};

inline void keccakF1600(uint64_t state[25]) {
    for (int round = 0; round < 24; ++round) {
        // Theta
        uint64_t C[5];
        for (int x = 0; x < 5; ++x) {
            C[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }
        uint64_t D[5];
        for (int x = 0; x < 5; ++x) {
            D[x] = C[(x + 4) % 5] ^ std::rotl(C[(x + 1) % 5], 1);
        }
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] ^= D[x];
            }
        }
        // Rho and Pi
        uint64_t B[25];
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                B[y + 5 * ((2 * x + 3 * y) % 5)] = std::rotl(state[x + 5 * y], KECCAK_ROTC[x][y]);
            }
        }
        // Chi
        for (int x = 0; x < 5; ++x) {
            for (int y = 0; y < 5; ++y) {
                state[x + 5 * y] = B[x + 5 * y] ^ ((~B[((x + 1) % 5) + 5 * y]) & B[((x + 2) % 5) + 5 * y]);
            }
        }
        // Iota
        state[0] ^= KECCAK_RC[round];
    }
}

struct Sha3Core {
    uint64_t state[25]{0};
    uint8_t buffer[200]{0};
    size_t buffer_len{0};
    size_t rate_bytes{136};  // 136 bytes for SHA3-256, 72 bytes for SHA3-512
    size_t output_bytes{32}; // 32 bytes for SHA3-256, 64 bytes for SHA3-512

    explicit Sha3Core(size_t out_len) : output_bytes(out_len) {
        rate_bytes = (1600 - (output_bytes * 8 * 2)) / 8;
    }

    void absorbBlock(const uint8_t* block) {
        for (size_t i = 0; i < rate_bytes; i += 8) {
            uint64_t word = 0;
            for (size_t b = 0; b < 8; ++b) {
                word |= static_cast<uint64_t>(block[i + b]) << (b * 8);
            }
            state[i / 8] ^= word;
        }
        keccakF1600(state);
    }

    void update(const void* data, size_t length) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        while (length > 0) {
            size_t to_copy = std::min(length, rate_bytes - buffer_len);
            std::memcpy(buffer + buffer_len, ptr, to_copy);
            buffer_len += to_copy;
            ptr += to_copy;
            length -= to_copy;
            if (buffer_len == rate_bytes) {
                absorbBlock(buffer);
                buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        // SHA-3 domain separation pad: 0x06 (0110 in bits), pad10*1
        buffer[buffer_len++] = 0x06;
        std::memset(buffer + buffer_len, 0, rate_bytes - buffer_len);
        buffer[rate_bytes - 1] |= 0x80;
        absorbBlock(buffer);

        std::string digest(output_bytes, '\0');
        size_t extracted = 0;
        for (size_t i = 0; i < 25 && extracted < output_bytes; ++i) {
            uint64_t word = state[i];
            for (size_t b = 0; b < 8 && extracted < output_bytes; ++b) {
                digest[extracted++] = static_cast<char>((word >> (b * 8)) & 0xFF);
            }
        }
        return digest;
    }

    void reset() {
        std::memset(state, 0, sizeof(state));
        buffer_len = 0;
    }
};

// ----------------------------------------------------------------------------
// BLAKE3 Implementation (Official Specification)
// ----------------------------------------------------------------------------
inline constexpr uint32_t BLAKE3_IV[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
};

inline constexpr uint8_t BLAKE3_SIGMA[7][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8},
    {3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1},
    {10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6},
    {12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
    {9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7},
    {11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13}
};

enum Blake3Flags : uint32_t {
    BLAKE3_CHUNK_START = 1 << 0,
    BLAKE3_CHUNK_END   = 1 << 1,
    BLAKE3_PARENT      = 1 << 2,
    BLAKE3_ROOT        = 1 << 3,
};

inline void blake3G(uint32_t state[16], int a, int b, int c, int d, uint32_t mx, uint32_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = std::rotr(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = std::rotr(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + my;
    state[d] = std::rotr(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = std::rotr(state[b] ^ state[c], 7);
}

inline void blake3Round(uint32_t state[16], const uint32_t m[16], const uint8_t schedule[16]) {
    blake3G(state, 0, 4, 8, 12, m[schedule[0]], m[schedule[1]]);
    blake3G(state, 1, 5, 9, 13, m[schedule[2]], m[schedule[3]]);
    blake3G(state, 2, 6, 10, 14, m[schedule[4]], m[schedule[5]]);
    blake3G(state, 3, 7, 11, 15, m[schedule[6]], m[schedule[7]]);
    blake3G(state, 0, 5, 10, 15, m[schedule[8]], m[schedule[9]]);
    blake3G(state, 1, 6, 11, 12, m[schedule[10]], m[schedule[11]]);
    blake3G(state, 2, 7, 8, 13, m[schedule[12]], m[schedule[13]]);
    blake3G(state, 3, 4, 9, 14, m[schedule[14]], m[schedule[15]]);
}

inline void blake3Compress(const uint32_t cv[8], const uint8_t block[64], uint8_t block_len,
                           uint64_t counter, uint32_t flags, uint32_t out[16]) {
    uint32_t state[16];
    std::memcpy(state, cv, 32);
    std::memcpy(state + 8, BLAKE3_IV, 16);
    state[12] = static_cast<uint32_t>(counter);
    state[13] = static_cast<uint32_t>(counter >> 32);
    state[14] = static_cast<uint32_t>(block_len);
    state[15] = flags;

    uint32_t m[16];
    for (int i = 0; i < 16; ++i) {
        m[i] = static_cast<uint32_t>(block[i * 4]) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 3]) << 24);
    }

    for (int r = 0; r < 7; ++r) {
        blake3Round(state, m, BLAKE3_SIGMA[r]);
    }

    for (int i = 0; i < 8; ++i) {
        out[i] = state[i] ^ state[i + 8];
        out[i + 8] = state[i + 8] ^ cv[i];
    }
}

struct Blake3Core {
    uint32_t chunk_cv[8];
    uint64_t chunk_counter{0};
    uint8_t block[64]{0};
    uint8_t block_len{0};
    uint8_t blocks_compressed{0};
    uint32_t flags{0};

    uint32_t cv_stack[54][8];
    size_t cv_stack_len{0};

    Blake3Core() {
        reset();
    }

    void reset() {
        std::memcpy(chunk_cv, BLAKE3_IV, sizeof(chunk_cv));
        chunk_counter = 0;
        block_len = 0;
        blocks_compressed = 0;
        flags = 0;
        cv_stack_len = 0;
    }

    void pushCv(const uint32_t new_cv[8], uint64_t total_chunks) {
        uint32_t current_cv[8];
        std::memcpy(current_cv, new_cv, 32);

        while ((total_chunks & 1) == 0) {
            uint8_t parent_block[64];
            for (int i = 0; i < 8; ++i) {
                uint32_t left = cv_stack[cv_stack_len - 1][i];
                parent_block[i * 4] = static_cast<uint8_t>(left);
                parent_block[i * 4 + 1] = static_cast<uint8_t>(left >> 8);
                parent_block[i * 4 + 2] = static_cast<uint8_t>(left >> 16);
                parent_block[i * 4 + 3] = static_cast<uint8_t>(left >> 24);
            }
            for (int i = 0; i < 8; ++i) {
                uint32_t right = current_cv[i];
                parent_block[32 + i * 4] = static_cast<uint8_t>(right);
                parent_block[32 + i * 4 + 1] = static_cast<uint8_t>(right >> 8);
                parent_block[32 + i * 4 + 2] = static_cast<uint8_t>(right >> 16);
                parent_block[32 + i * 4 + 3] = static_cast<uint8_t>(right >> 24);
            }
            cv_stack_len--;

            uint32_t out[16];
            blake3Compress(BLAKE3_IV, parent_block, 64, 0, BLAKE3_PARENT, out);
            std::memcpy(current_cv, out, 32);
            total_chunks >>= 1;
        }

        std::memcpy(cv_stack[cv_stack_len++], current_cv, 32);
    }

    void update(const void* data, size_t length) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        while (length > 0) {
            if (block_len == 64) {
                uint32_t chunk_flags = flags;
                if (blocks_compressed == 0) {
                    chunk_flags |= BLAKE3_CHUNK_START;
                }
                uint32_t out[16];
                blake3Compress(chunk_cv, block, 64, chunk_counter, chunk_flags, out);
                std::memcpy(chunk_cv, out, 32);
                blocks_compressed++;
                block_len = 0;
                std::memset(block, 0, sizeof(block));
            }

            if (blocks_compressed == 16) {
                // Chunk boundary (1024 bytes reached)
                chunk_counter++;
                pushCv(chunk_cv, chunk_counter);
                std::memcpy(chunk_cv, BLAKE3_IV, sizeof(chunk_cv));
                blocks_compressed = 0;
            }

            size_t to_copy = std::min(length, size_t{64} - block_len);
            std::memcpy(block + block_len, ptr, to_copy);
            block_len += static_cast<uint8_t>(to_copy);
            ptr += to_copy;
            length -= to_copy;
        }
    }

    std::string finalize() {
        uint32_t out[16];
        uint32_t chunk_flags = flags | BLAKE3_CHUNK_END;
        if (blocks_compressed == 0) {
            chunk_flags |= BLAKE3_CHUNK_START;
        }

        if (cv_stack_len == 0) {
            // Entire input fits in single chunk!
            chunk_flags |= BLAKE3_ROOT;
            blake3Compress(chunk_cv, block, block_len, chunk_counter, chunk_flags, out);
        } else {
            // Finish current chunk
            blake3Compress(chunk_cv, block, block_len, chunk_counter, chunk_flags, out);
            uint32_t current_cv[8];
            std::memcpy(current_cv, out, 32);

            // Merge up stack to single root
            size_t idx = cv_stack_len;
            while (idx > 0) {
                uint8_t parent_block[64];
                const uint32_t* left = cv_stack[idx - 1];
                for (int i = 0; i < 8; ++i) {
                    parent_block[i * 4] = static_cast<uint8_t>(left[i]);
                    parent_block[i * 4 + 1] = static_cast<uint8_t>(left[i] >> 8);
                    parent_block[i * 4 + 2] = static_cast<uint8_t>(left[i] >> 16);
                    parent_block[i * 4 + 3] = static_cast<uint8_t>(left[i] >> 24);
                }
                for (int i = 0; i < 8; ++i) {
                    parent_block[32 + i * 4] = static_cast<uint8_t>(current_cv[i]);
                    parent_block[32 + i * 4 + 1] = static_cast<uint8_t>(current_cv[i] >> 8);
                    parent_block[32 + i * 4 + 2] = static_cast<uint8_t>(current_cv[i] >> 16);
                    parent_block[32 + i * 4 + 3] = static_cast<uint8_t>(current_cv[i] >> 24);
                }
                idx--;
                uint32_t pflags = BLAKE3_PARENT;
                if (idx == 0) {
                    pflags |= BLAKE3_ROOT;
                }
                blake3Compress(BLAKE3_IV, parent_block, 64, 0, pflags, out);
                std::memcpy(current_cv, out, 32);
            }
        }

        std::string digest(32, '\0');
        for (int i = 0; i < 8; ++i) {
            digest[i * 4] = static_cast<char>(out[i] & 0xFF);
            digest[i * 4 + 1] = static_cast<char>((out[i] >> 8) & 0xFF);
            digest[i * 4 + 2] = static_cast<char>((out[i] >> 16) & 0xFF);
            digest[i * 4 + 3] = static_cast<char>((out[i] >> 24) & 0xFF);
        }
        return digest;
    }
};

} // namespace detail

// ============================================================================
// One-Shot Hash Functions
// ============================================================================

/// Computes SHA-256 hash of input string and returns lowercase hexadecimal string.
inline std::string sha256(const std::string& data) {
    detail::Sha256Core core;
    core.update(data.data(), data.size());
    return toHex(core.finalize());
}

/// Computes SHA-512 hash of input string and returns lowercase hexadecimal string.
inline std::string sha512(const std::string& data) {
    detail::Sha512Core core;
    core.update(data.data(), data.size());
    return toHex(core.finalize());
}

/// Computes SHA3-256 hash of input string and returns lowercase hexadecimal string.
inline std::string sha3_256(const std::string& data) {
    detail::Sha3Core core(32);
    core.update(data.data(), data.size());
    return toHex(core.finalize());
}

/// Computes SHA3-512 hash of input string and returns lowercase hexadecimal string.
inline std::string sha3_512(const std::string& data) {
    detail::Sha3Core core(64);
    core.update(data.data(), data.size());
    return toHex(core.finalize());
}

/// Computes BLAKE3 hash of input string and returns lowercase hexadecimal string.
inline std::string blake3(const std::string& data) {
    detail::Blake3Core core;
    core.update(data.data(), data.size());
    return toHex(core.finalize());
}

// ============================================================================
// Phase 2.2: Streaming Digest API (crypto.Hasher)
// ============================================================================

class Hasher {
public:
    virtual ~Hasher() = default;
    virtual void update(const std::string& chunk) = 0;
    virtual std::string digestBytes() = 0;
    virtual std::string digestHex() {
        return toHex(digestBytes());
    }
    virtual void reset() = 0;
};

namespace detail {

class Sha256Hasher : public Hasher {
    Sha256Core core_;
    bool finalized_{false};
    std::string digest_;
public:
    void update(const std::string& chunk) override {
        if (finalized_) throw keet::error("CryptoError", "Hasher already finalized");
        core_.update(chunk.data(), chunk.size());
    }
    std::string digestBytes() override {
        if (!finalized_) {
            digest_ = core_.finalize();
            finalized_ = true;
        }
        return digest_;
    }
    void reset() override {
        core_.reset();
        finalized_ = false;
        digest_.clear();
    }
};

class Sha512Hasher : public Hasher {
    Sha512Core core_;
    bool finalized_{false};
    std::string digest_;
public:
    void update(const std::string& chunk) override {
        if (finalized_) throw keet::error("CryptoError", "Hasher already finalized");
        core_.update(chunk.data(), chunk.size());
    }
    std::string digestBytes() override {
        if (!finalized_) {
            digest_ = core_.finalize();
            finalized_ = true;
        }
        return digest_;
    }
    void reset() override {
        core_.reset();
        finalized_ = false;
        digest_.clear();
    }
};

class Sha3Hasher : public Hasher {
    Sha3Core core_;
    bool finalized_{false};
    std::string digest_;
public:
    explicit Sha3Hasher(size_t out_len) : core_(out_len) {}
    void update(const std::string& chunk) override {
        if (finalized_) throw keet::error("CryptoError", "Hasher already finalized");
        core_.update(chunk.data(), chunk.size());
    }
    std::string digestBytes() override {
        if (!finalized_) {
            digest_ = core_.finalize();
            finalized_ = true;
        }
        return digest_;
    }
    void reset() override {
        core_.reset();
        finalized_ = false;
        digest_.clear();
    }
};

class Blake3Hasher : public Hasher {
    Blake3Core core_;
    bool finalized_{false};
    std::string digest_;
public:
    void update(const std::string& chunk) override {
        if (finalized_) throw keet::error("CryptoError", "Hasher already finalized");
        core_.update(chunk.data(), chunk.size());
    }
    std::string digestBytes() override {
        if (!finalized_) {
            digest_ = core_.finalize();
            finalized_ = true;
        }
        return digest_;
    }
    void reset() override {
        core_.reset();
        finalized_ = false;
        digest_.clear();
    }
};

} // namespace detail

/// Creates a streaming cryptographic hasher for the given algorithm name.
inline std::shared_ptr<Hasher> createHash(std::string algo) {
    for (char& c : algo) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    if (algo == "sha256" || algo == "sha-256") {
        return std::make_shared<detail::Sha256Hasher>();
    }
    if (algo == "sha512" || algo == "sha-512") {
        return std::make_shared<detail::Sha512Hasher>();
    }
    if (algo == "sha3-256" || algo == "sha3_256") {
        return std::make_shared<detail::Sha3Hasher>(32);
    }
    if (algo == "sha3-512" || algo == "sha3_512") {
        return std::make_shared<detail::Sha3Hasher>(64);
    }
    if (algo == "blake3") {
        return std::make_shared<detail::Blake3Hasher>();
    }
    throw keet::error("CryptoError", "Unsupported cryptographic hash algorithm: '" + algo + "'");
}

// ============================================================================
// Phase 2.3: Message Authentication Codes (HMAC)
// ============================================================================

namespace detail {

inline size_t getHashBlockSize(std::string_view algo) {
    if (algo == "sha256" || algo == "sha-256") return 64;
    if (algo == "sha512" || algo == "sha-512") return 128;
    if (algo == "sha3-256" || algo == "sha3_256") return 136;
    if (algo == "sha3-512" || algo == "sha3_512") return 72;
    if (algo == "blake3") return 64;
    return 64;
}

} // namespace detail

/// Streaming HMAC message authenticator (RFC 2104).
class Hmac {
    std::shared_ptr<Hasher> inner_;
    std::string k_opad_;
    std::string algorithm_;
    bool finalized_{false};
    std::string digest_;
public:
    Hmac(std::string algorithm, const std::string& key) : algorithm_(std::move(algorithm)) {
        size_t b = detail::getHashBlockSize(algorithm_);
        std::string k = key;
        if (k.size() > b) {
            auto h = createHash(algorithm_);
            h->update(k);
            k = h->digestBytes();
        }
        std::string k_pad(b, '\0');
        std::memcpy(k_pad.data(), k.data(), k.size());

        std::string k_ipad(b, '\0');
        k_opad_.assign(b, '\0');
        for (size_t i = 0; i < b; ++i) {
            k_ipad[i] = static_cast<char>(k_pad[i] ^ 0x36);
            k_opad_[i] = static_cast<char>(k_pad[i] ^ 0x5C);
        }

        inner_ = createHash(algorithm_);
        inner_->update(k_ipad);
    }

    void update(const std::string& chunk) {
        if (finalized_) throw keet::error("CryptoError", "Hmac already finalized");
        inner_->update(chunk);
    }

    std::string digestBytes() {
        if (!finalized_) {
            std::string inner_digest = inner_->digestBytes();
            auto outer = createHash(algorithm_);
            outer->update(k_opad_);
            outer->update(inner_digest);
            digest_ = outer->digestBytes();
            finalized_ = true;
        }
        return digest_;
    }

    std::string digestHex() {
        return toHex(digestBytes());
    }
};

/// Factory function to create a streaming HMAC engine.
inline Hmac createHmac(const std::string& algorithm, const std::string& key) {
    return Hmac(algorithm, key);
}

/// Computes HMAC-SHA256 message authentication code and returns lowercase hex string.
inline std::string hmacSha256(const std::string& key, const std::string& message) {
    Hmac h("sha256", key);
    h.update(message);
    return h.digestHex();
}

/// Computes HMAC-SHA512 message authentication code and returns lowercase hex string.
inline std::string hmacSha512(const std::string& key, const std::string& message) {
    Hmac h("sha512", key);
    h.update(message);
    return h.digestHex();
}

} // namespace keet::crypto

