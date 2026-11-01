#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

namespace keet::time {

// ============================================================================
// Time Unit Conversion Constants (int64_t)
// ============================================================================

inline constexpr int64_t NS_PER_US = 1'000LL;
inline constexpr int64_t NS_PER_MS = 1'000'000LL;
inline constexpr int64_t NS_PER_SEC = 1'000'000'000LL;
inline constexpr int64_t US_PER_MS = 1'000LL;
inline constexpr int64_t US_PER_SEC = 1'000'000LL;
inline constexpr int64_t MS_PER_SEC = 1'000LL;
inline constexpr int64_t SEC_PER_MIN = 60LL;
inline constexpr int64_t SEC_PER_HOUR = 3'600LL;
inline constexpr int64_t SEC_PER_DAY = 86'400LL;

// ============================================================================
// Detail: Cross-Platform Reentrant Helpers
// ============================================================================

namespace detail {

inline bool keet_gmtime(const std::time_t* sec, std::tm* result) noexcept {
#if defined(_WIN32)
    return gmtime_s(result, sec) == 0;
#else
    return gmtime_r(sec, result) != nullptr;
#endif
}

inline bool keet_localtime(const std::time_t* sec, std::tm* result) noexcept {
#if defined(_WIN32)
    return localtime_s(result, sec) == 0;
#else
    return localtime_r(sec, result) != nullptr;
#endif
}

inline std::time_t epoch_ms_to_sec(int64_t epochMs) noexcept {
    return static_cast<std::time_t>((epochMs >= 0) ? (epochMs / 1000) : ((epochMs - 999) / 1000));
}

} // namespace detail

// ============================================================================
// Wall-Clock Time Queries (std::chrono::system_clock)
// ============================================================================

[[nodiscard]] inline int64_t nowMs() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline int64_t now() noexcept {
    return nowMs();
}

[[nodiscard]] inline int64_t nowSec() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline int64_t nowUs() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline int64_t nowNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Monotonic High-Resolution Timers (std::chrono::steady_clock)
// ============================================================================

[[nodiscard]] inline int64_t monotonicMs() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline int64_t monotonicUs() noexcept {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline int64_t monotonicNs() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

[[nodiscard]] inline double monotonicSec() noexcept {
    return static_cast<double>(monotonicNs()) / 1e9;
}

[[nodiscard]] inline int64_t elapsedMs(int64_t startMs) noexcept {
    return monotonicMs() - startMs;
}

[[nodiscard]] inline double elapsedSec(int64_t startNs) noexcept {
    return static_cast<double>(monotonicNs() - startNs) / 1e9;
}

// ============================================================================
// Thread Sleep & Delay Operations
// ============================================================================

inline void sleepMs(int64_t ms) {
    if (ms < 0) {
        throw keet::error("TimeError", "sleepMs: duration cannot be negative: " + std::to_string(ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void sleep(int64_t ms) {
    if (ms < 0) {
        throw keet::error("TimeError", "sleep: duration cannot be negative: " + std::to_string(ms));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void sleepUs(int64_t us) {
    if (us < 0) {
        throw keet::error("TimeError", "sleepUs: duration cannot be negative: " + std::to_string(us));
    }
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void sleepSec(double seconds) {
    if (std::isnan(seconds) || std::isinf(seconds) || seconds < 0.0) {
        throw keet::error("TimeError", "sleepSec: duration must be non-negative finite number");
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

// ============================================================================
// Timestamp Formatting & Calendar Extraction
// ============================================================================

[[nodiscard]] inline std::string formatIso8601(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "formatIso8601: failed to convert timestamp to UTC calendar time");
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                  tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
    return std::string(buf);
}

[[nodiscard]] inline std::string formatUtc(int64_t epochMs, const std::string& pattern) {
    if (pattern.empty()) {
        throw keet::error("TimeError", "formatUtc: pattern cannot be empty");
    }
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "formatUtc: failed to convert timestamp to UTC calendar time");
    }
    size_t buf_size = std::max<size_t>(128, pattern.size() * 4);
    std::string buf(buf_size, '\0');
    size_t len = std::strftime(buf.data(), buf.size(), pattern.c_str(), &tm_val);
    if (len == 0) {
        buf.resize(buf_size * 4);
        len = std::strftime(buf.data(), buf.size(), pattern.c_str(), &tm_val);
        if (len == 0) {
            throw keet::error("TimeError", "formatUtc: formatting failed for pattern: " + pattern);
        }
    }
    buf.resize(len);
    return buf;
}

[[nodiscard]] inline std::string formatLocal(int64_t epochMs, const std::string& pattern) {
    if (pattern.empty()) {
        throw keet::error("TimeError", "formatLocal: pattern cannot be empty");
    }
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_localtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "formatLocal: failed to convert timestamp to local calendar time");
    }
    size_t buf_size = std::max<size_t>(128, pattern.size() * 4);
    std::string buf(buf_size, '\0');
    size_t len = std::strftime(buf.data(), buf.size(), pattern.c_str(), &tm_val);
    if (len == 0) {
        buf.resize(buf_size * 4);
        len = std::strftime(buf.data(), buf.size(), pattern.c_str(), &tm_val);
        if (len == 0) {
            throw keet::error("TimeError", "formatLocal: formatting failed for pattern: " + pattern);
        }
    }
    buf.resize(len);
    return buf;
}

[[nodiscard]] inline int64_t yearUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "yearUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_year + 1900);
}

[[nodiscard]] inline int64_t monthUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "monthUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_mon + 1);
}

[[nodiscard]] inline int64_t dayUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "dayUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_mday);
}

[[nodiscard]] inline int64_t hourUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "hourUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_hour);
}

[[nodiscard]] inline int64_t minuteUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "minuteUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_min);
}

[[nodiscard]] inline int64_t secondUtc(int64_t epochMs) {
    std::tm tm_val{};
    std::time_t sec = detail::epoch_ms_to_sec(epochMs);
    if (!detail::keet_gmtime(&sec, &tm_val)) {
        throw keet::error("TimeError", "secondUtc: failed to convert timestamp to UTC calendar time");
    }
    return static_cast<int64_t>(tm_val.tm_sec);
}

} // namespace keet::time
