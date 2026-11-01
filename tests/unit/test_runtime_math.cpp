#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <keet/math.hpp>
#include <keet/error.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("keet::math constants", "[runtime][math]") {
    REQUIRE_THAT(keet::math::PI, WithinRel(3.141592653589793, 1e-12));
    REQUIRE_THAT(keet::math::E, WithinRel(2.718281828459045, 1e-12));
    REQUIRE_THAT(keet::math::TAU, WithinRel(6.283185307179586, 1e-12));
    REQUIRE(keet::math::isInf(keet::math::INF));
    REQUIRE(keet::math::isNan(keet::math::NAN));
}

TEST_CASE("keet::math basic arithmetic and rounding", "[runtime][math]") {
    SECTION("abs and absInt") {
        REQUIRE_THAT(keet::math::abs(-4.5), WithinAbs(4.5, 1e-9));
        REQUIRE_THAT(keet::math::abs(4.5), WithinAbs(4.5, 1e-9));
        REQUIRE(keet::math::absInt(-42) == 42);
        REQUIRE(keet::math::absInt(42) == 42);
        REQUIRE(keet::math::abs(-100LL) == 100LL);
    }

    SECTION("min, minInt, max, maxInt") {
        REQUIRE_THAT(keet::math::min(3.5, 2.1), WithinAbs(2.1, 1e-9));
        REQUIRE(keet::math::minInt(10, 20) == 10);
        REQUIRE_THAT(keet::math::max(3.5, 2.1), WithinAbs(3.5, 1e-9));
        REQUIRE(keet::math::maxInt(10, 20) == 20);
        REQUIRE(keet::math::min(5LL, 9LL) == 5LL);
        REQUIRE(keet::math::max(5LL, 9LL) == 9LL);
    }

    SECTION("clamp and clampInt valid and invalid bounds") {
        REQUIRE_THAT(keet::math::clamp(5.0, 0.0, 10.0), WithinAbs(5.0, 1e-9));
        REQUIRE_THAT(keet::math::clamp(-2.0, 0.0, 10.0), WithinAbs(0.0, 1e-9));
        REQUIRE_THAT(keet::math::clamp(15.0, 0.0, 10.0), WithinAbs(10.0, 1e-9));
        REQUIRE(keet::math::clampInt(5, 0, 10) == 5);
        REQUIRE(keet::math::clampInt(-2, 0, 10) == 0);
        REQUIRE(keet::math::clampInt(15, 0, 10) == 10);
        REQUIRE(keet::math::clamp(7LL, 0LL, 10LL) == 7LL);

        // Invalid bounds throw MathError
        REQUIRE_THROWS_AS(keet::math::clamp(5.0, 10.0, 0.0), keet::error);
        REQUIRE_THROWS_AS(keet::math::clampInt(5, 10, 0), keet::error);
        try {
            (void)keet::math::clamp(5.0, 10.0, 0.0);
        } catch (const keet::error& err) {
            REQUIRE(err.name == "MathError");
        }
    }

    SECTION("sign and signInt") {
        REQUIRE_THAT(keet::math::sign(42.5), WithinAbs(1.0, 1e-9));
        REQUIRE_THAT(keet::math::sign(-42.5), WithinAbs(-1.0, 1e-9));
        REQUIRE_THAT(keet::math::sign(0.0), WithinAbs(0.0, 1e-9));
        REQUIRE(keet::math::signInt(42) == 1);
        REQUIRE(keet::math::signInt(-42) == -1);
        REQUIRE(keet::math::signInt(0) == 0);
        REQUIRE(keet::math::sign(10LL) == 1LL);
    }

    SECTION("rounding functions") {
        REQUIRE_THAT(keet::math::floor(3.7), WithinAbs(3.0, 1e-9));
        REQUIRE_THAT(keet::math::floor(-3.7), WithinAbs(-4.0, 1e-9));
        REQUIRE_THAT(keet::math::ceil(3.2), WithinAbs(4.0, 1e-9));
        REQUIRE_THAT(keet::math::ceil(-3.2), WithinAbs(-3.0, 1e-9));
        REQUIRE_THAT(keet::math::round(3.5), WithinAbs(4.0, 1e-9));
        REQUIRE_THAT(keet::math::round(3.4), WithinAbs(3.0, 1e-9));
        REQUIRE_THAT(keet::math::trunc(3.9), WithinAbs(3.0, 1e-9));
        REQUIRE_THAT(keet::math::trunc(-3.9), WithinAbs(-3.0, 1e-9));
    }
}

TEST_CASE("keet::math exponential, logarithmic, and power functions", "[runtime][math]") {
    REQUIRE_THAT(keet::math::sqrt(16.0), WithinAbs(4.0, 1e-9));
    REQUIRE_THAT(keet::math::cbrt(27.0), WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(keet::math::pow(2.0, 10.0), WithinAbs(1024.0, 1e-9));
    REQUIRE_THAT(keet::math::hypot(3.0, 4.0), WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(keet::math::exp(1.0), WithinRel(keet::math::E, 1e-9));
    REQUIRE_THAT(keet::math::log(keet::math::E), WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(keet::math::log2(8.0), WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(keet::math::log10(1000.0), WithinAbs(3.0, 1e-9));
}

TEST_CASE("keet::math trigonometry and angle conversions", "[runtime][math]") {
    REQUIRE_THAT(keet::math::sin(0.0), WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(keet::math::cos(0.0), WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(keet::math::tan(0.0), WithinAbs(0.0, 1e-9));

    REQUIRE_THAT(keet::math::toRadians(180.0), WithinRel(keet::math::PI, 1e-9));
    REQUIRE_THAT(keet::math::toDegrees(keet::math::PI), WithinAbs(180.0, 1e-9));

    REQUIRE_THAT(keet::math::asin(1.0), WithinRel(keet::math::PI / 2.0, 1e-9));
    REQUIRE_THAT(keet::math::acos(1.0), WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(keet::math::atan(1.0), WithinRel(keet::math::PI / 4.0, 1e-9));
    REQUIRE_THAT(keet::math::atan2(1.0, 1.0), WithinRel(keet::math::PI / 4.0, 1e-9));

    REQUIRE_THAT(keet::math::sinh(0.0), WithinAbs(0.0, 1e-9));
    REQUIRE_THAT(keet::math::cosh(0.0), WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(keet::math::tanh(0.0), WithinAbs(0.0, 1e-9));
}

TEST_CASE("keet::math interpolation and floating-point classification", "[runtime][math]") {
    REQUIRE_THAT(keet::math::lerp(10.0, 20.0, 0.5), WithinAbs(15.0, 1e-9));
    REQUIRE_THAT(keet::math::lerp(10.0, 20.0, 0.0), WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(keet::math::lerp(10.0, 20.0, 1.0), WithinAbs(20.0, 1e-9));

    REQUIRE_THAT(keet::math::fmod(7.5, 2.0), WithinAbs(1.5, 1e-9));

    REQUIRE(keet::math::isNan(keet::math::NAN));
    REQUIRE_FALSE(keet::math::isNan(42.0));
    REQUIRE(keet::math::isInf(keet::math::INF));
    REQUIRE_FALSE(keet::math::isInf(42.0));
    REQUIRE(keet::math::isFinite(42.0));
    REQUIRE_FALSE(keet::math::isFinite(keet::math::INF));
    REQUIRE_FALSE(keet::math::isFinite(keet::math::NAN));
}

TEST_CASE("keet::math pseudo-random number generator", "[runtime][math]") {
    SECTION("random returns [0.0, 1.0)") {
        keet::math::seed(12345);
        for (int i = 0; i < 20; ++i) {
            double r = keet::math::random();
            REQUIRE(r >= 0.0);
            REQUIRE(r < 1.0);
        }
    }

    SECTION("randomInt generates values within range") {
        keet::math::seed(999);
        for (int i = 0; i < 50; ++i) {
            int64_t val = keet::math::randomInt(5, 15);
            REQUIRE(val >= 5);
            REQUIRE(val <= 15);
        }
    }

    SECTION("randomFloat generates values within range") {
        keet::math::seed(999);
        for (int i = 0; i < 50; ++i) {
            double val = keet::math::randomFloat(2.5, 7.5);
            REQUIRE(val >= 2.5);
            REQUIRE(val < 7.5);
        }
    }

    SECTION("seed produces deterministic sequence") {
        keet::math::seed(42);
        double a1 = keet::math::random();
        int64_t b1 = keet::math::randomInt(1, 100);
        double c1 = keet::math::randomFloat(0.0, 10.0);

        keet::math::seed(42);
        double a2 = keet::math::random();
        int64_t b2 = keet::math::randomInt(1, 100);
        double c2 = keet::math::randomFloat(0.0, 10.0);

        REQUIRE_THAT(a1, WithinAbs(a2, 1e-12));
        REQUIRE(b1 == b2);
        REQUIRE_THAT(c1, WithinAbs(c2, 1e-12));
    }

    SECTION("invalid ranges throw MathError") {
        REQUIRE_THROWS_AS(keet::math::randomInt(10, 5), keet::error);
        REQUIRE_THROWS_AS(keet::math::randomFloat(10.0, 5.0), keet::error);
        try {
            (void)keet::math::randomInt(10, 5);
        } catch (const keet::error& err) {
            REQUIRE(err.name == "MathError");
        }
    }
}
