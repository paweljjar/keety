#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>

// Undefine conflicting C macros from <cmath> or system headers
#if defined(NAN)
#undef NAN
#endif
#if defined(INFINITY)
#undef INFINITY
#endif
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

namespace keet::math {

// ============================================================================
// Mathematical Constants
// ============================================================================

inline constexpr double PI = 3.14159265358979323846;
inline constexpr double E = 2.71828182845904523536;
inline constexpr double TAU = 6.28318530717958647692;
inline const double INF = std::numeric_limits<double>::infinity();
inline const double NAN = std::numeric_limits<double>::quiet_NaN();

// ============================================================================
// Basic & Rounding Operations
// ============================================================================

[[nodiscard]] inline double abs(double x) noexcept {
    return std::fabs(x);
}

[[nodiscard]] inline int64_t abs(int64_t x) noexcept {
    return std::abs(x);
}

[[nodiscard]] inline int64_t absInt(int64_t x) noexcept {
    return std::abs(x);
}

[[nodiscard]] inline double min(double a, double b) noexcept {
    return std::fmin(a, b);
}

[[nodiscard]] inline int64_t min(int64_t a, int64_t b) noexcept {
    return std::min(a, b);
}

[[nodiscard]] inline int64_t minInt(int64_t a, int64_t b) noexcept {
    return std::min(a, b);
}

[[nodiscard]] inline double max(double a, double b) noexcept {
    return std::fmax(a, b);
}

[[nodiscard]] inline int64_t max(int64_t a, int64_t b) noexcept {
    return std::max(a, b);
}

[[nodiscard]] inline int64_t maxInt(int64_t a, int64_t b) noexcept {
    return std::max(a, b);
}

[[nodiscard]] inline double clamp(double val, double minVal, double maxVal) {
    if (minVal > maxVal) {
        throw keet::error("MathError", "clamp: minVal cannot be greater than maxVal");
    }
    return std::clamp(val, minVal, maxVal);
}

[[nodiscard]] inline int64_t clamp(int64_t val, int64_t minVal, int64_t maxVal) {
    if (minVal > maxVal) {
        throw keet::error("MathError", "clamp: minVal cannot be greater than maxVal");
    }
    return std::clamp(val, minVal, maxVal);
}

[[nodiscard]] inline int64_t clampInt(int64_t val, int64_t minVal, int64_t maxVal) {
    if (minVal > maxVal) {
        throw keet::error("MathError", "clampInt: minVal cannot be greater than maxVal");
    }
    return std::clamp(val, minVal, maxVal);
}

[[nodiscard]] inline double floor(double x) noexcept {
    return std::floor(x);
}

[[nodiscard]] inline double ceil(double x) noexcept {
    return std::ceil(x);
}

[[nodiscard]] inline double round(double x) noexcept {
    return std::round(x);
}

[[nodiscard]] inline double trunc(double x) noexcept {
    return std::trunc(x);
}

[[nodiscard]] inline double sign(double x) noexcept {
    if (std::isnan(x)) return NAN;
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return 0.0;
}

[[nodiscard]] inline int64_t sign(int64_t x) noexcept {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

[[nodiscard]] inline int64_t signInt(int64_t x) noexcept {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

// ============================================================================
// Powers, Roots, Exponential & Logarithmic Functions
// ============================================================================

[[nodiscard]] inline double sqrt(double x) noexcept {
    return std::sqrt(x);
}

[[nodiscard]] inline double cbrt(double x) noexcept {
    return std::cbrt(x);
}

[[nodiscard]] inline double pow(double base, double exp) noexcept {
    return std::pow(base, exp);
}

[[nodiscard]] inline double hypot(double x, double y) noexcept {
    return std::hypot(x, y);
}

[[nodiscard]] inline double exp(double x) noexcept {
    return std::exp(x);
}

[[nodiscard]] inline double log(double x) noexcept {
    return std::log(x);
}

[[nodiscard]] inline double log2(double x) noexcept {
    return std::log2(x);
}

[[nodiscard]] inline double log10(double x) noexcept {
    return std::log10(x);
}

// ============================================================================
// Trigonometric & Hyperbolic Functions
// ============================================================================

[[nodiscard]] inline double sin(double x) noexcept {
    return std::sin(x);
}

[[nodiscard]] inline double cos(double x) noexcept {
    return std::cos(x);
}

[[nodiscard]] inline double tan(double x) noexcept {
    return std::tan(x);
}

[[nodiscard]] inline double asin(double x) noexcept {
    return std::asin(x);
}

[[nodiscard]] inline double acos(double x) noexcept {
    return std::acos(x);
}

[[nodiscard]] inline double atan(double x) noexcept {
    return std::atan(x);
}

[[nodiscard]] inline double atan2(double y, double x) noexcept {
    return std::atan2(y, x);
}

[[nodiscard]] inline double sinh(double x) noexcept {
    return std::sinh(x);
}

[[nodiscard]] inline double cosh(double x) noexcept {
    return std::cosh(x);
}

[[nodiscard]] inline double tanh(double x) noexcept {
    return std::tanh(x);
}

[[nodiscard]] inline double toRadians(double deg) noexcept {
    return deg * (PI / 180.0);
}

[[nodiscard]] inline double toDegrees(double rad) noexcept {
    return rad * (180.0 / PI);
}

// ============================================================================
// Floating-Point Classification & Utility Functions
// ============================================================================

[[nodiscard]] inline bool isNan(double x) noexcept {
    return std::isnan(x);
}

[[nodiscard]] inline bool isInf(double x) noexcept {
    return std::isinf(x);
}

[[nodiscard]] inline bool isFinite(double x) noexcept {
    return std::isfinite(x);
}

[[nodiscard]] inline double lerp(double a, double b, double t) noexcept {
#if defined(__cpp_lib_interpolate) && __cpp_lib_interpolate >= 201902L
    return std::lerp(a, b, t);
#else
    return a + t * (b - a);
#endif
}

[[nodiscard]] inline double fmod(double x, double y) noexcept {
    return std::fmod(x, y);
}

// ============================================================================
// Pseudo-Random Number Generation (Thread-Safe <random>)
// ============================================================================

namespace detail {
inline std::mt19937_64& get_prng() {
    thread_local std::mt19937_64 engine([]() {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    }());
    return engine;
}
} // namespace detail

inline void seed(int64_t s) noexcept {
    detail::get_prng().seed(static_cast<uint64_t>(s));
}

[[nodiscard]] inline double random() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(detail::get_prng());
}

[[nodiscard]] inline int64_t randomInt(int64_t minVal, int64_t maxVal) {
    if (minVal > maxVal) {
        throw keet::error("MathError", "randomInt: minVal cannot be greater than maxVal");
    }
    std::uniform_int_distribution<int64_t> dist(minVal, maxVal);
    return dist(detail::get_prng());
}

[[nodiscard]] inline double randomFloat(double minVal, double maxVal) {
    if (minVal > maxVal) {
        throw keet::error("MathError", "randomFloat: minVal cannot be greater than maxVal");
    }
    std::uniform_real_distribution<double> dist(minVal, maxVal);
    return dist(detail::get_prng());
}

} // namespace keet::math
