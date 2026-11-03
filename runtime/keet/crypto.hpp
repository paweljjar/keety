#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <array>
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

template <typename T>
[[nodiscard]] constexpr T rotr(T val, unsigned int shift) noexcept {
    constexpr unsigned int width = sizeof(T) * 8;
    shift &= (width - 1);
    if (shift == 0) return val;
    return static_cast<T>((val >> shift) | (val << (width - shift)));
}

template <typename T>
[[nodiscard]] constexpr T rotl(T val, unsigned int shift) noexcept {
    constexpr unsigned int width = sizeof(T) * 8;
    shift &= (width - 1);
    if (shift == 0) return val;
    return static_cast<T>((val << shift) | (val >> (width - shift)));
}

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
            uint32_t s0 = rotr(W[t - 15], 7) ^ rotr(W[t - 15], 18) ^ (W[t - 15] >> 3);
            uint32_t s1 = rotr(W[t - 2], 17) ^ rotr(W[t - 2], 19) ^ (W[t - 2] >> 10);
            W[t] = W[t - 16] + s0 + W[t - 7] + s1;
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int t = 0; t < 64; ++t) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + K[t] + W[t];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
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
            uint64_t s0 = rotr(W[t - 15], 1) ^ rotr(W[t - 15], 8) ^ (W[t - 15] >> 7);
            uint64_t s1 = rotr(W[t - 2], 19) ^ rotr(W[t - 2], 61) ^ (W[t - 2] >> 6);
            W[t] = W[t - 16] + s0 + W[t - 7] + s1;
        }

        uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

        for (int t = 0; t < 80; ++t) {
            uint64_t S1 = rotr(e, 14) ^ rotr(e, 18) ^ rotr(e, 41);
            uint64_t ch = (e & f) ^ ((~e) & g);
            uint64_t temp1 = h + S1 + ch + K[t] + W[t];
            uint64_t S0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39);
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
            D[x] = C[(x + 4) % 5] ^ rotl(C[(x + 1) % 5], 1);
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
                B[y + 5 * ((2 * x + 3 * y) % 5)] = rotl(state[x + 5 * y], KECCAK_ROTC[x][y]);
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
    state[d] = rotr(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = rotr(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + my;
    state[d] = rotr(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = rotr(state[b] ^ state[c], 7);
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

// ============================================================================
// Phase 3.1: Data Structures & Contracts (EncryptedPayload)
// ============================================================================

/// Container for AEAD authenticated encryption output (ciphertext, tag, IV, optional AAD).
struct EncryptedPayload {
    std::string ciphertext;
    std::string tag;
    std::string iv;
    std::string aad;

    /// Serializes IV, Tag, and Ciphertext into colon-separated hex format (ivHex:tagHex:ctHex).
    std::string toCombinedHex() const {
        return toHex(iv) + ":" + toHex(tag) + ":" + toHex(ciphertext);
    }

    /// Deserializes colon-separated hex format back into an EncryptedPayload.
    static EncryptedPayload fromCombinedHex(const std::string& combined, const std::string& aad = "") {
        auto first_colon = combined.find(':');
        if (first_colon == std::string::npos) {
            throw keet::error("CryptoError", "Invalid combined encrypted hex payload: missing ':' delimiter");
        }
        auto second_colon = combined.find(':', first_colon + 1);
        if (second_colon == std::string::npos) {
            throw keet::error("CryptoError", "Invalid combined encrypted hex payload: missing second ':' delimiter");
        }
        EncryptedPayload p;
        p.iv = fromHex(combined.substr(0, first_colon));
        p.tag = fromHex(combined.substr(first_colon + 1, second_colon - first_colon - 1));
        p.ciphertext = fromHex(combined.substr(second_colon + 1));
        p.aad = aad;
        return p;
    }
};

// ============================================================================
// Phase 3.2: ChaCha20-Poly1305 (RFC 8439)
// ============================================================================

namespace detail {

inline uint32_t load32Le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
          (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}

inline void store32Le(uint8_t* p, uint32_t v) noexcept {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

inline uint64_t load64Be(const uint8_t* p) noexcept {
    return (static_cast<uint64_t>(p[0]) << 56) |
           (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) |
           (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) |
           (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) << 8) |
           (static_cast<uint64_t>(p[7]));
}

inline void store64Be(uint8_t* p, uint64_t v) noexcept {
    p[0] = static_cast<uint8_t>(v >> 56);
    p[1] = static_cast<uint8_t>(v >> 48);
    p[2] = static_cast<uint8_t>(v >> 40);
    p[3] = static_cast<uint8_t>(v >> 32);
    p[4] = static_cast<uint8_t>(v >> 24);
    p[5] = static_cast<uint8_t>(v >> 16);
    p[6] = static_cast<uint8_t>(v >> 8);
    p[7] = static_cast<uint8_t>(v);
}

inline void chachaQR(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) noexcept {
    a += b; d ^= a; d = rotl(d, 16);
    c += d; b ^= c; b = rotl(b, 12);
    a += b; d ^= a; d = rotl(d, 8);
    c += d; b ^= c; b = rotl(b, 7);
}

inline void chacha20Block(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12], uint8_t out[64]) noexcept {
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        load32Le(key),      load32Le(key + 4),  load32Le(key + 8),  load32Le(key + 12),
        load32Le(key + 16), load32Le(key + 20), load32Le(key + 24), load32Le(key + 28),
        counter,
        load32Le(nonce),    load32Le(nonce + 4), load32Le(nonce + 8)
    };
    uint32_t working[16];
    std::memcpy(working, state, sizeof(state));

    for (int i = 0; i < 10; ++i) {
        chachaQR(working[0], working[4], working[8],  working[12]);
        chachaQR(working[1], working[5], working[9],  working[13]);
        chachaQR(working[2], working[6], working[10], working[14]);
        chachaQR(working[3], working[7], working[11], working[15]);
        chachaQR(working[0], working[5], working[10], working[15]);
        chachaQR(working[1], working[6], working[11], working[12]);
        chachaQR(working[2], working[7], working[8],  working[13]);
        chachaQR(working[3], working[4], working[9],  working[14]);
    }

    for (int i = 0; i < 16; ++i) {
        store32Le(out + i * 4, working[i] + state[i]);
    }
}

inline std::string chacha20Xor(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12],
                               const uint8_t* data, size_t len) {
    std::string res;
    res.resize(len);
    uint8_t block[64];
    for (size_t i = 0; i < len; i += 64) {
        chacha20Block(key, counter++, nonce, block);
        size_t chunk = std::min(len - i, size_t{64});
        for (size_t j = 0; j < chunk; ++j) {
            res[i + j] = static_cast<char>(data[i + j] ^ block[j]);
        }
    }
    return res;
}

// ----------------------------------------------------------------------------
// Poly1305 26-bit limb constant-time accumulator (modulo 2^130 - 5)
// ----------------------------------------------------------------------------
struct Poly1305 {
    uint32_t r0{0}, r1{0}, r2{0}, r3{0}, r4{0};
    uint32_t s1{0}, s2{0}, s3{0}, s4{0};
    uint32_t h0{0}, h1{0}, h2{0}, h3{0}, h4{0};
    uint8_t pad[16]{0};
    uint8_t buffer[16]{0};
    size_t buffer_len{0};

    explicit Poly1305(const uint8_t key[32]) {
        // Clamp r
        uint32_t t0 = load32Le(key)       & 0x0fffffff;
        uint32_t t1 = load32Le(key + 4)   & 0x0ffffffc;
        uint32_t t2 = load32Le(key + 8)   & 0x0ffffffc;
        uint32_t t3 = load32Le(key + 12)  & 0x0ffffffc;

        r0 = t0 & 0x03ffffff;
        r1 = ((t0 >> 26) | (t1 << 6)) & 0x03ffffff;
        r2 = ((t1 >> 20) | (t2 << 12)) & 0x03ffffff;
        r3 = ((t2 >> 14) | (t3 << 18)) & 0x03ffffff;
        r4 = (t3 >> 8) & 0x03ffffff;

        s1 = r1 * 5;
        s2 = r2 * 5;
        s3 = r3 * 5;
        s4 = r4 * 5;

        std::memcpy(pad, key + 16, 16);
    }

    void processBlock(const uint8_t* block, size_t bytes) {
        uint8_t mp[17]{0};
        std::memcpy(mp, block, bytes);
        mp[bytes] = 1;

        uint32_t t0 = load32Le(mp);
        uint32_t t1 = load32Le(mp + 4);
        uint32_t t2 = load32Le(mp + 8);
        uint32_t t3 = load32Le(mp + 12);
        uint32_t t4 = mp[16];

        uint32_t m0 = t0 & 0x03ffffff;
        uint32_t m1 = ((t0 >> 26) | (t1 << 6)) & 0x03ffffff;
        uint32_t m2 = ((t1 >> 20) | (t2 << 12)) & 0x03ffffff;
        uint32_t m3 = ((t2 >> 14) | (t3 << 18)) & 0x03ffffff;
        uint32_t m4 = (t3 >> 8) | (t4 << 24);

        h0 += m0;
        h1 += m1;
        h2 += m2;
        h3 += m3;
        h4 += m4;

        uint64_t d0 = static_cast<uint64_t>(h0) * r0 + static_cast<uint64_t>(h1) * s4 +
                      static_cast<uint64_t>(h2) * s3 + static_cast<uint64_t>(h3) * s2 +
                      static_cast<uint64_t>(h4) * s1;
        uint64_t d1 = static_cast<uint64_t>(h0) * r1 + static_cast<uint64_t>(h1) * r0 +
                      static_cast<uint64_t>(h2) * s4 + static_cast<uint64_t>(h3) * s3 +
                      static_cast<uint64_t>(h4) * s2;
        uint64_t d2 = static_cast<uint64_t>(h0) * r2 + static_cast<uint64_t>(h1) * r1 +
                      static_cast<uint64_t>(h2) * r0 + static_cast<uint64_t>(h3) * s4 +
                      static_cast<uint64_t>(h4) * s3;
        uint64_t d3 = static_cast<uint64_t>(h0) * r3 + static_cast<uint64_t>(h1) * r2 +
                      static_cast<uint64_t>(h2) * r1 + static_cast<uint64_t>(h3) * r0 +
                      static_cast<uint64_t>(h4) * s4;
        uint64_t d4 = static_cast<uint64_t>(h0) * r4 + static_cast<uint64_t>(h1) * r3 +
                      static_cast<uint64_t>(h2) * r2 + static_cast<uint64_t>(h3) * r1 +
                      static_cast<uint64_t>(h4) * r0;

        uint64_t c = d0 >> 26; h0 = static_cast<uint32_t>(d0) & 0x03ffffff; d1 += c;
        c = d1 >> 26; h1 = static_cast<uint32_t>(d1) & 0x03ffffff; d2 += c;
        c = d2 >> 26; h2 = static_cast<uint32_t>(d2) & 0x03ffffff; d3 += c;
        c = d3 >> 26; h3 = static_cast<uint32_t>(d3) & 0x03ffffff; d4 += c;
        c = d4 >> 26; h4 = static_cast<uint32_t>(d4) & 0x03ffffff; h0 += static_cast<uint32_t>(c * 5);
        c = h0 >> 26; h0 &= 0x03ffffff; h1 += static_cast<uint32_t>(c);
    }

    void update(const uint8_t* data, size_t len) {
        while (len > 0) {
            size_t to_copy = std::min(len, size_t{16} - buffer_len);
            std::memcpy(buffer + buffer_len, data, to_copy);
            buffer_len += to_copy;
            data += to_copy;
            len -= to_copy;
            if (buffer_len == 16) {
                processBlock(buffer, 16);
                buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        if (buffer_len > 0) {
            processBlock(buffer, buffer_len);
            buffer_len = 0;
        }

        uint32_t c = h1 >> 26; h1 &= 0x03ffffff; h2 += c;
        c = h2 >> 26; h2 &= 0x03ffffff; h3 += c;
        c = h3 >> 26; h3 &= 0x03ffffff; h4 += c;
        c = h4 >> 26; h4 &= 0x03ffffff; h0 += c * 5;
        c = h0 >> 26; h0 &= 0x03ffffff; h1 += c;

        // Compute h + -p = h + 5
        uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x03ffffff;
        uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x03ffffff;
        uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x03ffffff;
        uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x03ffffff;
        uint32_t g4 = h4 + c - (1 << 26);

        // Select h if h < p else g
        uint32_t mask = (g4 >> 31) - 1;
        g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
        mask = ~mask;
        h0 = (h0 & mask) | g0;
        h1 = (h1 & mask) | g1;
        h2 = (h2 & mask) | g2;
        h3 = (h3 & mask) | g3;
        h4 = (h4 & mask) | g4;

        // Reconstruct 128-bit integer
        uint32_t f0 = (h0) | (h1 << 26);
        uint32_t f1 = (h1 >> 6) | (h2 << 20);
        uint32_t f2 = (h2 >> 12) | (h3 << 14);
        uint32_t f3 = (h3 >> 18) | (h4 << 8);

        // Add pad
        uint64_t carry = static_cast<uint64_t>(f0) + load32Le(pad);
        f0 = static_cast<uint32_t>(carry);
        carry = static_cast<uint64_t>(f1) + load32Le(pad + 4) + (carry >> 32);
        f1 = static_cast<uint32_t>(carry);
        carry = static_cast<uint64_t>(f2) + load32Le(pad + 8) + (carry >> 32);
        f2 = static_cast<uint32_t>(carry);
        carry = static_cast<uint64_t>(f3) + load32Le(pad + 12) + (carry >> 32);
        f3 = static_cast<uint32_t>(carry);

        std::string tag(16, '\0');
        store32Le(reinterpret_cast<uint8_t*>(tag.data()), f0);
        store32Le(reinterpret_cast<uint8_t*>(tag.data()) + 4, f1);
        store32Le(reinterpret_cast<uint8_t*>(tag.data()) + 8, f2);
        store32Le(reinterpret_cast<uint8_t*>(tag.data()) + 12, f3);
        return tag;
    }
};

} // namespace detail

/// Encrypts plaintext using ChaCha20-Poly1305 authenticated encryption (RFC 8439).
inline EncryptedPayload encryptChaCha20Poly1305(const std::string& key, const std::string& nonce,
                                                const std::string& plaintext, const std::string& aad = "") {
    if (key.size() != 32) {
        throw keet::error("CryptoError", "ChaCha20 key must be exactly 32 bytes (got " + std::to_string(key.size()) + ")");
    }
    if (nonce.size() != 12) {
        throw keet::error("CryptoError", "ChaCha20 nonce must be exactly 12 bytes (got " + std::to_string(nonce.size()) + ")");
    }

    const uint8_t* k_ptr = reinterpret_cast<const uint8_t*>(key.data());
    const uint8_t* n_ptr = reinterpret_cast<const uint8_t*>(nonce.data());

    // Generate 32-byte Poly1305 key with counter = 0
    uint8_t poly_key[64];
    detail::chacha20Block(k_ptr, 0, n_ptr, poly_key);

    // Encrypt plaintext with counter = 1
    std::string ciphertext = detail::chacha20Xor(k_ptr, 1, n_ptr,
                                                 reinterpret_cast<const uint8_t*>(plaintext.data()),
                                                 plaintext.size());

    // Compute Poly1305 tag over AAD || pad || ciphertext || pad || len(AAD) || len(ciphertext)
    detail::Poly1305 poly(poly_key);
    if (!aad.empty()) {
        poly.update(reinterpret_cast<const uint8_t*>(aad.data()), aad.size());
        if (aad.size() % 16 != 0) {
            uint8_t zero_pad[16]{0};
            poly.update(zero_pad, 16 - (aad.size() % 16));
        }
    }
    if (!ciphertext.empty()) {
        poly.update(reinterpret_cast<const uint8_t*>(ciphertext.data()), ciphertext.size());
        if (ciphertext.size() % 16 != 0) {
            uint8_t zero_pad[16]{0};
            poly.update(zero_pad, 16 - (ciphertext.size() % 16));
        }
    }

    uint8_t len_bytes[16];
    detail::store32Le(len_bytes, static_cast<uint32_t>(aad.size()));
    detail::store32Le(len_bytes + 4, static_cast<uint32_t>(static_cast<uint64_t>(aad.size()) >> 32));
    detail::store32Le(len_bytes + 8, static_cast<uint32_t>(ciphertext.size()));
    detail::store32Le(len_bytes + 12, static_cast<uint32_t>(static_cast<uint64_t>(ciphertext.size()) >> 32));
    poly.update(len_bytes, 16);

    std::string tag = poly.finalize();

    EncryptedPayload payload;
    payload.ciphertext = std::move(ciphertext);
    payload.tag = std::move(tag);
    payload.iv = nonce;
    payload.aad = aad;
    return payload;
}

/// Decrypts ciphertext using ChaCha20-Poly1305 authenticated encryption (RFC 8439).
inline std::string decryptChaCha20Poly1305(const std::string& key, const std::string& nonce,
                                          const std::string& ciphertext, const std::string& tag,
                                          const std::string& aad = "") {
    if (key.size() != 32) {
        throw keet::error("CryptoError", "ChaCha20 key must be exactly 32 bytes");
    }
    if (nonce.size() != 12) {
        throw keet::error("CryptoError", "ChaCha20 nonce must be exactly 12 bytes");
    }
    if (tag.size() != 16) {
        throw keet::error("CryptoError", "Poly1305 authentication tag must be exactly 16 bytes");
    }

    const uint8_t* k_ptr = reinterpret_cast<const uint8_t*>(key.data());
    const uint8_t* n_ptr = reinterpret_cast<const uint8_t*>(nonce.data());

    uint8_t poly_key[64];
    detail::chacha20Block(k_ptr, 0, n_ptr, poly_key);

    detail::Poly1305 poly(poly_key);
    if (!aad.empty()) {
        poly.update(reinterpret_cast<const uint8_t*>(aad.data()), aad.size());
        if (aad.size() % 16 != 0) {
            uint8_t zero_pad[16]{0};
            poly.update(zero_pad, 16 - (aad.size() % 16));
        }
    }
    if (!ciphertext.empty()) {
        poly.update(reinterpret_cast<const uint8_t*>(ciphertext.data()), ciphertext.size());
        if (ciphertext.size() % 16 != 0) {
            uint8_t zero_pad[16]{0};
            poly.update(zero_pad, 16 - (ciphertext.size() % 16));
        }
    }

    uint8_t len_bytes[16];
    detail::store32Le(len_bytes, static_cast<uint32_t>(aad.size()));
    detail::store32Le(len_bytes + 4, static_cast<uint32_t>(static_cast<uint64_t>(aad.size()) >> 32));
    detail::store32Le(len_bytes + 8, static_cast<uint32_t>(ciphertext.size()));
    detail::store32Le(len_bytes + 12, static_cast<uint32_t>(static_cast<uint64_t>(ciphertext.size()) >> 32));
    poly.update(len_bytes, 16);

    std::string expected_tag = poly.finalize();
    if (!timingSafeEqual(expected_tag, tag)) {
        throw keet::error("CryptoError", "Authentication tag verification failed (ciphertext tampered or incorrect key)");
    }

    return detail::chacha20Xor(k_ptr, 1, n_ptr,
                              reinterpret_cast<const uint8_t*>(ciphertext.data()),
                              ciphertext.size());
}

// ============================================================================
// Phase 3.3: AES-256-GCM (NIST SP 800-38D)
// ============================================================================

namespace detail {

inline constexpr uint8_t AES_SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

inline constexpr uint8_t AES_RCON[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

inline uint32_t aesSubWord(uint32_t w) noexcept {
    return (static_cast<uint32_t>(AES_SBOX[(w >> 24) & 0xFF]) << 24) |
           (static_cast<uint32_t>(AES_SBOX[(w >> 16) & 0xFF]) << 16) |
           (static_cast<uint32_t>(AES_SBOX[(w >> 8) & 0xFF]) << 8) |
           (static_cast<uint32_t>(AES_SBOX[w & 0xFF]));
}

inline uint8_t aesXtime(uint8_t a) noexcept {
    return (a & 0x80) ? static_cast<uint8_t>((a << 1) ^ 0x1b) : static_cast<uint8_t>(a << 1);
}

inline void aes256KeyExpansion(const uint8_t key[32], uint32_t round_keys[60]) noexcept {
    for (int i = 0; i < 8; ++i) {
        round_keys[i] = (static_cast<uint32_t>(key[i * 4]) << 24) |
                        (static_cast<uint32_t>(key[i * 4 + 1]) << 16) |
                        (static_cast<uint32_t>(key[i * 4 + 2]) << 8) |
                        (static_cast<uint32_t>(key[i * 4 + 3]));
    }
    for (int i = 8; i < 60; ++i) {
        uint32_t temp = round_keys[i - 1];
        if (i % 8 == 0) {
            temp = aesSubWord(rotl(temp, 8)) ^ (static_cast<uint32_t>(AES_RCON[(i / 8) - 1]) << 24);
        } else if (i % 8 == 4) {
            temp = aesSubWord(temp);
        }
        round_keys[i] = round_keys[i - 8] ^ temp;
    }
}

inline void aesEncryptBlock(const uint8_t pt[16], const uint32_t round_keys[60], uint8_t ct[16]) noexcept {
    uint8_t state[16];
    std::memcpy(state, pt, 16);

    for (int i = 0; i < 16; ++i) {
        state[i] ^= static_cast<uint8_t>((round_keys[i / 4] >> (24 - 8 * (i % 4))) & 0xFF);
    }

    for (int r = 1; r <= 14; ++r) {
        for (int i = 0; i < 16; ++i) {
            state[i] = AES_SBOX[state[i]];
        }
        uint8_t t0 = state[0], t1 = state[5], t2 = state[10], t3 = state[15];
        uint8_t t4 = state[4], t5 = state[9], t6 = state[14], t7 = state[3];
        uint8_t t8 = state[8], t9 = state[13], t10 = state[2], t11 = state[7];
        uint8_t t12 = state[12], t13 = state[1], t14 = state[6], t15 = state[11];

        state[0] = t0; state[1] = t1; state[2] = t2; state[3] = t3;
        state[4] = t4; state[5] = t5; state[6] = t6; state[7] = t7;
        state[8] = t8; state[9] = t9; state[10] = t10; state[11] = t11;
        state[12] = t12; state[13] = t13; state[14] = t14; state[15] = t15;

        if (r < 14) {
            for (int c = 0; c < 4; ++c) {
                uint8_t a0 = state[c * 4];
                uint8_t a1 = state[c * 4 + 1];
                uint8_t a2 = state[c * 4 + 2];
                uint8_t a3 = state[c * 4 + 3];
                uint8_t t = a0 ^ a1 ^ a2 ^ a3;
                state[c * 4]     = a0 ^ t ^ aesXtime(a0 ^ a1);
                state[c * 4 + 1] = a1 ^ t ^ aesXtime(a1 ^ a2);
                state[c * 4 + 2] = a2 ^ t ^ aesXtime(a2 ^ a3);
                state[c * 4 + 3] = a3 ^ t ^ aesXtime(a3 ^ a0);
            }
        }

        for (int i = 0; i < 16; ++i) {
            state[i] ^= static_cast<uint8_t>((round_keys[r * 4 + (i / 4)] >> (24 - 8 * (i % 4))) & 0xFF);
        }
    }
    std::memcpy(ct, state, 16);
}

inline void gf128Multiply(const uint8_t x[16], const uint8_t y[16], uint8_t out[16]) noexcept {
    uint64_t z_hi = 0, z_lo = 0;
    uint64_t v_hi = load64Be(y);
    uint64_t v_lo = load64Be(y + 8);
    constexpr uint64_t R = 0xe100000000000000ULL;

    for (int i = 0; i < 16; ++i) {
        uint8_t b = x[i];
        for (int bit = 7; bit >= 0; --bit) {
            if ((b >> bit) & 1) {
                z_hi ^= v_hi;
                z_lo ^= v_lo;
            }
            bool lsb = (v_lo & 1) != 0;
            v_lo = ((v_lo >> 1) | ((v_hi & 1) << 63));
            v_hi >>= 1;
            if (lsb) {
                v_hi ^= R;
            }
        }
    }
    store64Be(out, z_hi);
    store64Be(out + 8, z_lo);
}

inline void ghashBlock(uint8_t y[16], const uint8_t block[16], const uint8_t h[16]) noexcept {
    uint8_t xor_block[16];
    for (int i = 0; i < 16; ++i) {
        xor_block[i] = y[i] ^ block[i];
    }
    gf128Multiply(xor_block, h, y);
}

} // namespace detail

/// Encrypts plaintext using AES-256-GCM authenticated encryption (NIST SP 800-38D).
inline EncryptedPayload encryptAesGcm(const std::string& key, const std::string& iv,
                                      const std::string& plaintext, const std::string& aad = "") {
    if (key.size() != 32) {
        throw keet::error("CryptoError", "AES-256 key must be exactly 32 bytes (got " + std::to_string(key.size()) + ")");
    }
    if (iv.size() != 12) {
        throw keet::error("CryptoError", "AES-GCM IV must be exactly 12 bytes (got " + std::to_string(iv.size()) + ")");
    }

    const uint8_t* k_ptr = reinterpret_cast<const uint8_t*>(key.data());
    uint32_t round_keys[60];
    detail::aes256KeyExpansion(k_ptr, round_keys);

    // Compute H = AES(0^16)
    uint8_t zero_block[16]{0};
    uint8_t H[16];
    detail::aesEncryptBlock(zero_block, round_keys, H);

    // J0 = IV || 0^31 1
    uint8_t J0[16];
    std::memcpy(J0, iv.data(), 12);
    J0[12] = 0; J0[13] = 0; J0[14] = 0; J0[15] = 1;

    // Tag mask = AES(J0)
    uint8_t tag_mask[16];
    detail::aesEncryptBlock(J0, round_keys, tag_mask);

    // CTR1 = inc32(J0)
    uint8_t ctr[16];
    std::memcpy(ctr, J0, 16);
    ctr[15] = 2; // 1 + 1 = 2

    // AES-CTR encrypt plaintext
    std::string ciphertext;
    ciphertext.resize(plaintext.size());
    uint8_t stream[16];
    for (size_t i = 0; i < plaintext.size(); i += 16) {
        detail::aesEncryptBlock(ctr, round_keys, stream);
        size_t chunk = std::min(plaintext.size() - i, size_t{16});
        for (size_t j = 0; j < chunk; ++j) {
            ciphertext[i + j] = static_cast<char>(plaintext[i + j] ^ stream[j]);
        }
        // inc32(ctr)
        uint32_t c_val = (static_cast<uint32_t>(ctr[12]) << 24) |
                         (static_cast<uint32_t>(ctr[13]) << 16) |
                         (static_cast<uint32_t>(ctr[14]) << 8) |
                         (static_cast<uint32_t>(ctr[15]));
        c_val++;
        ctr[12] = static_cast<uint8_t>(c_val >> 24);
        ctr[13] = static_cast<uint8_t>(c_val >> 16);
        ctr[14] = static_cast<uint8_t>(c_val >> 8);
        ctr[15] = static_cast<uint8_t>(c_val);
    }

    // GHASH over AAD || pad || ciphertext || pad || len(AAD)_bits || len(ciphertext)_bits
    uint8_t S[16]{0};
    for (size_t i = 0; i < aad.size(); i += 16) {
        uint8_t b[16]{0};
        size_t chunk = std::min(aad.size() - i, size_t{16});
        std::memcpy(b, aad.data() + i, chunk);
        detail::ghashBlock(S, b, H);
    }
    for (size_t i = 0; i < ciphertext.size(); i += 16) {
        uint8_t b[16]{0};
        size_t chunk = std::min(ciphertext.size() - i, size_t{16});
        std::memcpy(b, ciphertext.data() + i, chunk);
        detail::ghashBlock(S, b, H);
    }

    uint8_t len_block[16];
    detail::store64Be(len_block, static_cast<uint64_t>(aad.size()) * 8);
    detail::store64Be(len_block + 8, static_cast<uint64_t>(ciphertext.size()) * 8);
    detail::ghashBlock(S, len_block, H);

    // Tag = S ^ tag_mask
    std::string tag(16, '\0');
    for (int i = 0; i < 16; ++i) {
        tag[i] = static_cast<char>(S[i] ^ tag_mask[i]);
    }

    EncryptedPayload payload;
    payload.ciphertext = std::move(ciphertext);
    payload.tag = std::move(tag);
    payload.iv = iv;
    payload.aad = aad;
    return payload;
}

/// Decrypts ciphertext using AES-256-GCM authenticated encryption (NIST SP 800-38D).
inline std::string decryptAesGcm(const std::string& key, const std::string& iv,
                                const std::string& ciphertext, const std::string& tag,
                                const std::string& aad = "") {
    if (key.size() != 32) {
        throw keet::error("CryptoError", "AES-256 key must be exactly 32 bytes");
    }
    if (iv.size() != 12) {
        throw keet::error("CryptoError", "AES-GCM IV must be exactly 12 bytes");
    }
    if (tag.size() != 16) {
        throw keet::error("CryptoError", "AES-GCM authentication tag must be exactly 16 bytes");
    }

    const uint8_t* k_ptr = reinterpret_cast<const uint8_t*>(key.data());
    uint32_t round_keys[60];
    detail::aes256KeyExpansion(k_ptr, round_keys);

    uint8_t zero_block[16]{0};
    uint8_t H[16];
    detail::aesEncryptBlock(zero_block, round_keys, H);

    uint8_t J0[16];
    std::memcpy(J0, iv.data(), 12);
    J0[12] = 0; J0[13] = 0; J0[14] = 0; J0[15] = 1;

    uint8_t tag_mask[16];
    detail::aesEncryptBlock(J0, round_keys, tag_mask);

    uint8_t S[16]{0};
    for (size_t i = 0; i < aad.size(); i += 16) {
        uint8_t b[16]{0};
        size_t chunk = std::min(aad.size() - i, size_t{16});
        std::memcpy(b, aad.data() + i, chunk);
        detail::ghashBlock(S, b, H);
    }
    for (size_t i = 0; i < ciphertext.size(); i += 16) {
        uint8_t b[16]{0};
        size_t chunk = std::min(ciphertext.size() - i, size_t{16});
        std::memcpy(b, ciphertext.data() + i, chunk);
        detail::ghashBlock(S, b, H);
    }

    uint8_t len_block[16];
    detail::store64Be(len_block, static_cast<uint64_t>(aad.size()) * 8);
    detail::store64Be(len_block + 8, static_cast<uint64_t>(ciphertext.size()) * 8);
    detail::ghashBlock(S, len_block, H);

    std::string expected_tag(16, '\0');
    for (int i = 0; i < 16; ++i) {
        expected_tag[i] = static_cast<char>(S[i] ^ tag_mask[i]);
    }

    if (!timingSafeEqual(expected_tag, tag)) {
        throw keet::error("CryptoError", "Authentication tag verification failed (ciphertext tampered or incorrect key)");
    }

    // Decrypt ciphertext using AES-CTR
    uint8_t ctr[16];
    std::memcpy(ctr, J0, 16);
    ctr[15] = 2;

    std::string plaintext;
    plaintext.resize(ciphertext.size());
    uint8_t stream[16];
    for (size_t i = 0; i < ciphertext.size(); i += 16) {
        detail::aesEncryptBlock(ctr, round_keys, stream);
        size_t chunk = std::min(ciphertext.size() - i, size_t{16});
        for (size_t j = 0; j < chunk; ++j) {
            plaintext[i + j] = static_cast<char>(ciphertext[i + j] ^ stream[j]);
        }
        uint32_t c_val = (static_cast<uint32_t>(ctr[12]) << 24) |
                         (static_cast<uint32_t>(ctr[13]) << 16) |
                         (static_cast<uint32_t>(ctr[14]) << 8) |
                         (static_cast<uint32_t>(ctr[15]));
        c_val++;
        ctr[12] = static_cast<uint8_t>(c_val >> 24);
        ctr[13] = static_cast<uint8_t>(c_val >> 16);
        ctr[14] = static_cast<uint8_t>(c_val >> 8);
        ctr[15] = static_cast<uint8_t>(c_val);
    }

    return plaintext;
}

// ============================================================================
// Phase 4.1: 1D Barcodes (Code 128)
// ============================================================================

namespace detail {

// Code 128 bar/space patterns for symbols 0..106
// Each entry encodes 6 bar/space widths (except 106 which has 7 widths)
inline constexpr const char* CODE128_WIDTHS[107] = {
    "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312", "132212", "221213", // 0-9
    "221312", "231212", "112232", "122132", "122231", "113222", "123122", "123221", "223211", "221132", // 10-19
    "221231", "213212", "223112", "312131", "311222", "321122", "321221", "312212", "322112", "322211", // 20-29
    "212123", "212321", "232121", "111323", "131123", "131321", "112313", "132113", "132311", "211313", // 30-39
    "231113", "231311", "112133", "112331", "132131", "113123", "113321", "133121", "313121", "211331", // 40-49
    "231131", "213113", "213311", "213131", "311123", "311321", "331121", "312113", "312311", "332111", // 50-59
    "314111", "221411", "431111", "111224", "111422", "121124", "121421", "141122", "141221", "112214", // 60-69
    "112412", "122114", "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111", // 70-79
    "111242", "121142", "121241", "114212", "124112", "124211", "411212", "421112", "421211", "212141", // 80-89
    "214121", "412121", "111143", "111341", "131141", "114113", "114311", "411113", "411311", "113141", // 90-99
    "114131", "311141", "411131", "211412", "211214", "211232", "2331112" // 100-106 (104=Start B, 105=Start C, 106=Stop)
};

inline std::string widthsToModuleBits(const char* widths) {
    std::string bits;
    bool is_bar = true;
    for (const char* p = widths; *p != '\0'; ++p) {
        int w = *p - '0';
        bits.append(static_cast<size_t>(w), is_bar ? '1' : '0');
        is_bar = !is_bar;
    }
    return bits;
}

} // namespace detail

/// Encodes text into Code 128 binary module pattern ('1' for bar, '0' for space).
inline std::string encodeCode128(const std::string& text) {
    if (text.empty()) {
        throw keet::error("CryptoError", "Cannot encode empty text into Code 128 barcode");
    }

    std::vector<int> symbols;
    // Use Code B (ASCII 32..126) with Start B (104)
    symbols.push_back(104);
    for (char c : text) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126) {
            throw keet::error("CryptoError", "Character outside Code 128-B range (ASCII 32-126)");
        }
        symbols.push_back(uc - 32);
    }

    // Checksum modulo 103
    int64_t sum = symbols[0];
    for (size_t i = 1; i < symbols.size(); ++i) {
        sum += static_cast<int64_t>(i) * symbols[i];
    }
    int checksum = static_cast<int>(sum % 103);
    symbols.push_back(checksum);
    symbols.push_back(106); // Stop symbol

    // Convert symbols to 1/0 bits with quiet zones (10 modules on each side)
    std::string modules;
    modules.append(10, '0'); // Quiet zone
    for (int sym : symbols) {
        modules.append(detail::widthsToModuleBits(detail::CODE128_WIDTHS[sym]));
    }
    modules.append(10, '0'); // Quiet zone
    return modules;
}

/// Renders Code 128 barcode as a standalone vector SVG string.
inline std::string encodeCode128Svg(const std::string& text, int barHeight = 50, int moduleWidth = 2) {
    std::string bits = encodeCode128(text);
    int totalWidth = static_cast<int>(bits.size()) * moduleWidth;

    std::ostringstream oss;
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << totalWidth
        << "\" height=\"" << barHeight << "\" viewBox=\"0 0 " << totalWidth << " " << barHeight << "\">\n"
        << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

    size_t i = 0;
    while (i < bits.size()) {
        if (bits[i] == '1') {
            size_t start = i;
            while (i < bits.size() && bits[i] == '1') {
                i++;
            }
            int w = static_cast<int>(i - start) * moduleWidth;
            oss << "  <rect x=\"" << (static_cast<int>(start) * moduleWidth) << "\" y=\"0\" width=\""
                << w << "\" height=\"" << barHeight << "\" fill=\"#000000\"/>\n";
        } else {
            i++;
        }
    }
    oss << "</svg>";
    return oss.str();
}

/// Renders Code 128 barcode as a compact ASCII / Unicode terminal string.
inline std::string encodeCode128Ascii(const std::string& text) {
    std::string bits = encodeCode128(text);
    std::string out;
    out.reserve(bits.size() * 3);
    for (char b : bits) {
        if (b == '1') {
            out.append("█");
        } else {
            out.push_back(' ');
        }
    }
    return out;
}

/// Decodes a Code 128 module bitstream back into plain text.
inline std::string decodeCode128(const std::string& bitstream) {
    // Strip leading and trailing quiet zone zeros
    size_t start = bitstream.find('1');
    size_t end = bitstream.rfind('1');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        throw keet::error("CryptoError", "Invalid Code 128 bitstream: no bar patterns detected");
    }

    std::string trimmed = bitstream.substr(start, end - start + 1);

    // Map module pattern (11 bits) to symbol index
    std::vector<int> symbols;
    size_t pos = 0;
    while (pos < trimmed.size()) {
        if (pos + 13 == trimmed.size()) {
            // STOP pattern is 13 bits (symbol 106)
            std::string stop_pattern = detail::widthsToModuleBits(detail::CODE128_WIDTHS[106]);
            if (trimmed.substr(pos, 13) != stop_pattern) {
                throw keet::error("CryptoError", "Invalid Code 128 stop pattern");
            }
            symbols.push_back(106);
            pos += 13;
            break;
        }

        if (pos + 11 > trimmed.size()) {
            throw keet::error("CryptoError", "Truncated Code 128 symbol pattern");
        }

        std::string chunk = trimmed.substr(pos, 11);
        int matched = -1;
        for (int s = 0; s < 106; ++s) {
            if (detail::widthsToModuleBits(detail::CODE128_WIDTHS[s]) == chunk) {
                matched = s;
                break;
            }
        }
        if (matched < 0) {
            throw keet::error("CryptoError", "Unrecognized Code 128 module sequence at position " + std::to_string(pos));
        }
        symbols.push_back(matched);
        pos += 11;
    }

    if (symbols.size() < 3) {
        throw keet::error("CryptoError", "Code 128 pattern too short");
    }

    int start_code = symbols[0];
    if (start_code != 104) {
        throw keet::error("CryptoError", "Only Code 128-B decoding is currently supported (start code " +
                                             std::to_string(start_code) + ")");
    }
    if (symbols.back() != 106) {
        throw keet::error("CryptoError", "Missing Code 128 stop symbol");
    }

    int expected_checksum = symbols[symbols.size() - 2];
    int64_t sum = symbols[0];
    for (size_t i = 1; i < symbols.size() - 2; ++i) {
        sum += static_cast<int64_t>(i) * symbols[i];
    }
    int computed_checksum = static_cast<int>(sum % 103);
    if (computed_checksum != expected_checksum) {
        throw keet::error("CryptoError", "Code 128 checksum mismatch: expected " +
                                             std::to_string(expected_checksum) + ", computed " +
                                             std::to_string(computed_checksum));
    }

    std::string text;
    for (size_t i = 1; i < symbols.size() - 2; ++i) {
        text.push_back(static_cast<char>(symbols[i] + 32));
    }
    return text;
}

// ============================================================================
// Phase 4.2 & 4.3: QR Code (ISO/IEC 18004) Encoder & Decoder
// ============================================================================

/// Represents a generated or scanned QR Code matrix with rendering helpers.
struct QrCode {
    int size{0};
    int version{0};
    std::vector<std::vector<BoolByte>> matrix; // [y][x] (true = dark, false = light)

    /// Renders QR matrix as an SVG image string.
    std::string toSvg(int margin = 4) const {
        int total = size + margin * 2;
        std::ostringstream oss;
        oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << total * 10
            << "\" height=\"" << total * 10 << "\" viewBox=\"0 0 " << total << " " << total
            << "\" shape-rendering=\"crispEdges\">\n"
            << "  <rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (matrix[y][x]) {
                    oss << "  <rect x=\"" << (x + margin) << "\" y=\"" << (y + margin)
                        << "\" width=\"1\" height=\"1\" fill=\"#000000\"/>\n";
                }
            }
        }
        oss << "</svg>";
        return oss.str();
    }

    /// Renders QR code in terminal using Unicode half-block characters (2 rows per character).
    std::string toAscii(int margin = 2) const {
        int total = size + margin * 2;
        std::vector<std::vector<bool>> full(total, std::vector<bool>(total, false));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                full[y + margin][x + margin] = matrix[y][x];
            }
        }

        std::string out;
        for (int y = 0; y < total; y += 2) {
            for (int x = 0; x < total; ++x) {
                bool top = full[y][x];
                bool bot = (y + 1 < total) ? full[y + 1][x] : false;
                if (top && bot) {
                    out.append("█");
                } else if (top && !bot) {
                    out.append("▀");
                } else if (!top && bot) {
                    out.append("▄");
                } else {
                    out.push_back(' ');
                }
            }
            out.push_back('\n');
        }
        return out;
    }
};

namespace detail {

// Galois Field GF(256) tables for Reed-Solomon polynomial math
struct Gf256 {
    uint8_t exp[512]{0};
    uint8_t log[256]{0};

    constexpr Gf256() {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            exp[i] = static_cast<uint8_t>(x);
            exp[i + 255] = static_cast<uint8_t>(x);
            log[x] = static_cast<uint8_t>(i);
            x <<= 1;
            if (x & 0x100) {
                x ^= 0x11D; // Primitive polynomial x^8 + x^4 + x^3 + x^2 + 1
            }
        }
    }

    [[nodiscard]] constexpr uint8_t mul(uint8_t a, uint8_t b) const noexcept {
        if (a == 0 || b == 0) return 0;
        return exp[log[a] + log[b]];
    }
};

inline constexpr Gf256 GF256{};

// QR Version parameters (Versions 1..6 with Medium EC level)
struct QrVersionInfo {
    int version;
    int size;
    int data_codewords;
    int ec_codewords;
    int ec_blocks;
};

inline constexpr QrVersionInfo QR_VERSIONS_M[6] = {
    {1, 21, 16, 10, 1},
    {2, 25, 28, 16, 1},
    {3, 29, 44, 26, 1},
    {4, 33, 64, 36, 2},
    {5, 37, 86, 48, 2},
    {6, 41, 108, 64, 4}
};

// Alignment pattern centers for versions 1..6
inline const std::vector<int> QR_ALIGNMENT_LOCS[7] = {
    {},
    {},
    {6, 18},
    {6, 22},
    {6, 26},
    {6, 30},
    {6, 34}
};

inline std::vector<uint8_t> rsGeneratorPoly(int count) {
    std::vector<uint8_t> gen = {1};
    for (int i = 0; i < count; ++i) {
        std::vector<uint8_t> next_gen(gen.size() + 1, 0);
        for (size_t j = 0; j < gen.size(); ++j) {
            next_gen[j] ^= GF256.mul(gen[j], GF256.exp[i]);
            next_gen[j + 1] ^= gen[j];
        }
        gen = next_gen;
    }
    return gen;
}

inline std::vector<uint8_t> rsCompute(const std::vector<uint8_t>& data, int ec_len) {
    auto gen = rsGeneratorPoly(ec_len);
    std::vector<uint8_t> g_rev(gen.rbegin(), gen.rend());

    std::vector<uint8_t> rem(ec_len, 0);
    for (uint8_t byte : data) {
        uint8_t factor = byte ^ rem[0];
        rem.erase(rem.begin());
        rem.push_back(0);
        for (int j = 0; j < ec_len; ++j) {
            rem[j] ^= GF256.mul(g_rev[j + 1], factor);
        }
    }
    return rem;
}

inline bool isReservedModule(int x, int y, int size, int version) {
    // Top-left finder + separator
    if (x <= 8 && y <= 8) return true;
    // Top-right finder + separator
    if (x >= size - 8 && y <= 8) return true;
    // Bottom-left finder + separator
    if (x <= 8 && y >= size - 8) return true;
    // Timing patterns
    if (x == 6 || y == 6) return true;
    // Dark module
    if (x == 8 && y == 4 * version + 9) return true;

    // Alignment patterns
    if (version >= 2) {
        const auto& locs = QR_ALIGNMENT_LOCS[version];
        for (int ax : locs) {
            for (int ay : locs) {
                if ((ax <= 8 && ay <= 8) || (ax >= size - 8 && ay <= 8) || (ax <= 8 && ay >= size - 8)) {
                    continue; // Skip finders
                }
                if (std::abs(x - ax) <= 2 && std::abs(y - ay) <= 2) {
                    return true;
                }
            }
        }
    }
    return false;
}

inline bool maskBit(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return ((y / 2) + (x / 3)) % 2 == 0;
        case 5: return ((x * y) % 2) + ((x * y) % 3) == 0;
        case 6: return (((x * y) % 2) + ((x * y) % 3)) % 2 == 0;
        case 7: return (((x + y) % 2) + ((x * y) % 3)) % 2 == 0;
        default: return false;
    }
}

inline uint16_t computeFormatBits(int ec_level_bits, int mask) {
    int data = (ec_level_bits << 3) | mask; // 5 bits
    int rem = data << 10;
    constexpr int G = 0x537; // 10100110111
    for (int i = 4; i >= 0; --i) {
        if ((rem >> (i + 10)) & 1) {
            rem ^= (G << i);
        }
    }
    uint16_t format_bits = static_cast<uint16_t>(((data << 10) | rem) ^ 0x5412);
    return format_bits;
}

} // namespace detail

/// Generates an ISO/IEC 18004 compliant QR Code from text using Byte mode and Medium error correction.
inline QrCode generateQr(const std::string& text, const std::string& ecLevel = "M") {
    (void)ecLevel; // Currently standardized on Medium (15% error recovery)
    if (text.empty()) {
        throw keet::error("CryptoError", "QR code payload cannot be empty");
    }
    size_t payload_len = text.size();

    // Auto-select minimum QR version
    const detail::QrVersionInfo* v_info = nullptr;
    for (const auto& vi : detail::QR_VERSIONS_M) {
        // Byte mode overhead: 4 bits mode + 8 bits count = 12 bits (1.5 bytes) + terminator
        if (payload_len + 3 <= static_cast<size_t>(vi.data_codewords)) {
            v_info = &vi;
            break;
        }
    }
    if (!v_info) {
        throw keet::error("CryptoError", "Payload exceeds maximum supported QR code capacity (" +
                                             std::to_string(payload_len) + " bytes)");
    }

    int version = v_info->version;
    int size = v_info->size;
    int total_data = v_info->data_codewords;
    int ec_codewords = v_info->ec_codewords;

    // 1. Pack bitstream: Mode (0100 = Byte mode) + Count + Payload
    std::string bits;
    bits.append("0100"); // 4 bits Byte mode
    for (int i = 7; i >= 0; --i) {
        bits.push_back(((payload_len >> i) & 1) ? '1' : '0');
    }
    for (char c : text) {
        uint8_t byte = static_cast<uint8_t>(c);
        for (int i = 7; i >= 0; --i) {
            bits.push_back(((byte >> i) & 1) ? '1' : '0');
        }
    }

    // 4-bit terminator (or up to capacity)
    size_t max_bits = total_data * 8;
    size_t term = std::min(size_t{4}, max_bits - bits.size());
    bits.append(term, '0');
    // Align to byte
    while (bits.size() % 8 != 0 && bits.size() < max_bits) {
        bits.push_back('0');
    }

    // Convert bits to data codewords
    std::vector<uint8_t> data_codewords;
    data_codewords.reserve(total_data);
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t b = 0;
        for (size_t j = 0; j < 8; ++j) {
            b = (b << 1) | (bits[i + j] - '0');
        }
        data_codewords.push_back(b);
    }

    // Pad bytes 0xEC (236) and 0x11 (17) alternating
    uint8_t pad_byte = 0xEC;
    while (data_codewords.size() < static_cast<size_t>(total_data)) {
        data_codewords.push_back(pad_byte);
        pad_byte = (pad_byte == 0xEC) ? 0x11 : 0xEC;
    }

    // 2. Compute Reed-Solomon Error Correction
    std::vector<uint8_t> ec_bytes = detail::rsCompute(data_codewords, ec_codewords);

    // Combine data + EC codewords
    std::vector<uint8_t> all_codewords = data_codewords;
    all_codewords.insert(all_codewords.end(), ec_bytes.begin(), ec_bytes.end());

    std::string final_bits;
    final_bits.reserve(all_codewords.size() * 8);
    for (uint8_t c : all_codewords) {
        for (int i = 7; i >= 0; --i) {
            final_bits.push_back(((c >> i) & 1) ? '1' : '0');
        }
    }

    // 3. Initialize Matrix
    std::vector<std::vector<BoolByte>> matrix(size, std::vector<BoolByte>(size, false));

    // Draw Function Patterns:
    auto drawFinder = [&](int ox, int oy) {
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 7; ++x) {
                bool dark = (x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4));
                matrix[oy + y][ox + x] = dark;
            }
        }
    };
    drawFinder(0, 0);                 // Top-left
    drawFinder(size - 7, 0);          // Top-right
    drawFinder(0, size - 7);          // Bottom-left

    // Timing patterns
    for (int i = 8; i < size - 8; ++i) {
        matrix[6][i] = (i % 2 == 0);
        matrix[i][6] = (i % 2 == 0);
    }

    // Alignment patterns (if version >= 2)
    if (version >= 2) {
        const auto& locs = detail::QR_ALIGNMENT_LOCS[version];
        for (int ax : locs) {
            for (int ay : locs) {
                if ((ax <= 8 && ay <= 8) || (ax >= size - 8 && ay <= 8) || (ax <= 8 && ay >= size - 8)) continue;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        bool dark = (std::max(std::abs(dx), std::abs(dy)) != 1);
                        matrix[ay + dy][ax + dx] = dark;
                    }
                }
            }
        }
    }

    // Dark module
    matrix[4 * version + 9][8] = true;

    // 4. Place Data and evaluate mask (standard mask 0: (x+y)%2 == 0)
    int mask_choice = 0;
    size_t bit_idx = 0;
    bool upward = true;

    for (int right = size - 1; right > 0; right -= 2) {
        if (right == 6) {
            right--; // Skip vertical timing pattern column
        }
        for (int vert = 0; vert < size; ++vert) {
            int y = upward ? (size - 1 - vert) : vert;
            for (int col = 0; col < 2; ++col) {
                int x = right - col;
                if (!detail::isReservedModule(x, y, size, version)) {
                    bool data_bit = (bit_idx < final_bits.size()) ? (final_bits[bit_idx++] == '1') : false;
                    bool mask = detail::maskBit(mask_choice, x, y);
                    matrix[y][x] = data_bit ^ mask;
                }
            }
        }
        upward = !upward;
    }

    // 5. Draw Format Information (EC Level M = 00_2, Mask 0 = 000_2)
    uint16_t fmt = detail::computeFormatBits(0, mask_choice);

    // Place around top-left finder and divided across top-right / bottom-left
    int fmt_coords_tl[15][2] = {
        {8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 7}, {8, 8},
        {7, 8}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8}
    };
    for (int i = 0; i < 15; ++i) {
        bool bit = (fmt >> i) & 1;
        matrix[fmt_coords_tl[i][0]][fmt_coords_tl[i][1]] = bit;
    }

    for (int i = 0; i < 7; ++i) {
        matrix[size - 1 - i][8] = (fmt >> i) & 1;
    }
    for (int i = 7; i < 15; ++i) {
        matrix[8][size - 15 + i] = (fmt >> i) & 1;
    }

    QrCode qr;
    qr.size = size;
    qr.version = version;
    qr.matrix = std::move(matrix);
    return qr;
}

/// Scans a 2D QR Code matrix and decodes the embedded text payload.
inline std::string decodeQr(const std::vector<std::vector<BoolByte>>& matrix) {
    int size = static_cast<int>(matrix.size());
    if (size < 21 || (size - 17) % 4 != 0) {
        throw keet::error("CryptoError", "Invalid QR code matrix dimensions (" + std::to_string(size) + ")");
    }
    int version = (size - 17) / 4;

    // Read format information from around top-left finder
    int fmt_coords_tl[15][2] = {
        {8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 7}, {8, 8},
        {7, 8}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8}
    };
    uint16_t raw_fmt = 0;
    for (int i = 0; i < 15; ++i) {
        if (matrix[fmt_coords_tl[i][0]][fmt_coords_tl[i][1]]) {
            raw_fmt |= (1 << i);
        }
    }
    uint16_t unmasked_fmt = raw_fmt ^ 0x5412;
    int mask_choice = (unmasked_fmt >> 10) & 0x07;

    // Read modules in zigzag order with mask de-masking
    std::string bits;
    bool upward = true;
    for (int right = size - 1; right > 0; right -= 2) {
        if (right == 6) right--;
        for (int vert = 0; vert < size; ++vert) {
            int y = upward ? (size - 1 - vert) : vert;
            for (int col = 0; col < 2; ++col) {
                int x = right - col;
                if (!detail::isReservedModule(x, y, size, version)) {
                    bool val = matrix[y][x];
                    bool mask = detail::maskBit(mask_choice, x, y);
                    bits.push_back((val ^ mask) ? '1' : '0');
                }
            }
        }
        upward = !upward;
    }

    // Parse bitstream
    if (bits.size() < 12) {
        throw keet::error("CryptoError", "Truncated QR code data bitstream");
    }

    std::string mode_str = bits.substr(0, 4);
    if (mode_str != "0100") {
        throw keet::error("CryptoError", "Unsupported QR encoding mode (expected Byte mode '0100', got '" +
                                             mode_str + "')");
    }

    // 8-bit character count indicator for versions 1..9
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        count = (count << 1) | (bits[4 + i] - '0');
    }

    if (bits.size() < static_cast<size_t>(12 + count * 8)) {
        throw keet::error("CryptoError", "QR code bitstream truncated before payload completion");
    }

    std::string text;
    text.reserve(count);
    size_t pos = 12;
    for (int i = 0; i < count; ++i) {
        uint8_t b = 0;
        for (int j = 0; j < 8; ++j) {
            b = (b << 1) | (bits[pos++] - '0');
        }
        text.push_back(static_cast<char>(b));
    }
    return text;
}

} // namespace keet::crypto



