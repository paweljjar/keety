#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <keet/time.hpp>
#include <keet/error.hpp>

#include <cmath>
#include <limits>

using Catch::Matchers::WithinAbs;

TEST_CASE("keet::time conversion constants", "[runtime][time]") {
    REQUIRE(keet::time::NS_PER_US == 1000LL);
    REQUIRE(keet::time::NS_PER_MS == 1000000LL);
    REQUIRE(keet::time::NS_PER_SEC == 1000000000LL);
    REQUIRE(keet::time::US_PER_MS == 1000LL);
    REQUIRE(keet::time::US_PER_SEC == 1000000LL);
    REQUIRE(keet::time::MS_PER_SEC == 1000LL);
    REQUIRE(keet::time::SEC_PER_MIN == 60LL);
    REQUIRE(keet::time::SEC_PER_HOUR == 3600LL);
    REQUIRE(keet::time::SEC_PER_DAY == 86400LL);

    // Consistency relationships
    REQUIRE(keet::time::NS_PER_SEC == keet::time::NS_PER_MS * keet::time::MS_PER_SEC);
    REQUIRE(keet::time::US_PER_SEC == keet::time::US_PER_MS * keet::time::MS_PER_SEC);
    REQUIRE(keet::time::SEC_PER_HOUR == keet::time::SEC_PER_MIN * 60LL);
    REQUIRE(keet::time::SEC_PER_DAY == keet::time::SEC_PER_HOUR * 24LL);
}

TEST_CASE("keet::time wall-clock queries", "[runtime][time]") {
    int64_t ms1 = keet::time::now();
    int64_t ms2 = keet::time::nowMs();
    int64_t sec = keet::time::nowSec();
    int64_t us = keet::time::nowUs();
    int64_t ns = keet::time::nowNs();

    // Verify sensible values (after year 2023: 1.7e12 ms)
    REQUIRE(ms1 >= 1700000000000LL);
    REQUIRE(ms2 >= ms1);
    REQUIRE(sec >= 1700000000LL);
    REQUIRE(us >= ms1 * 1000LL);
    REQUIRE(ns >= us * 1000LL);

    // Relationship checks
    REQUIRE(std::abs(sec - ms1 / 1000LL) <= 1LL);
}

TEST_CASE("keet::time monotonic clock and elapsed timers", "[runtime][time]") {
    int64_t m1 = keet::time::monotonicMs();
    int64_t u1 = keet::time::monotonicUs();
    int64_t n1 = keet::time::monotonicNs();
    double s1 = keet::time::monotonicSec();

    REQUIRE(m1 >= 0);
    REQUIRE(u1 >= 0);
    REQUIRE(n1 >= 0);
    REQUIRE(s1 >= 0.0);

    keet::time::sleepMs(5);

    int64_t m2 = keet::time::monotonicMs();
    REQUIRE(m2 >= m1);

    int64_t elapsed_ms = keet::time::elapsedMs(m1);
    REQUIRE(elapsed_ms >= 4);

    double elapsed_s = keet::time::elapsedSec(n1);
    REQUIRE(elapsed_s >= 0.004);
}

TEST_CASE("keet::time sleep operations and domain validation", "[runtime][time]") {
    SECTION("Valid sleeps execute without error") {
        int64_t t0 = keet::time::monotonicMs();
        keet::time::sleep(8);
        int64_t elapsed = keet::time::elapsedMs(t0);
        REQUIRE(elapsed >= 6);

        keet::time::sleepUs(500);
        keet::time::sleepSec(0.005);
    }

    SECTION("Negative durations throw TimeError") {
        REQUIRE_THROWS_AS(keet::time::sleep(-1), keet::error);
        REQUIRE_THROWS_AS(keet::time::sleepMs(-10), keet::error);
        REQUIRE_THROWS_AS(keet::time::sleepUs(-100), keet::error);
        REQUIRE_THROWS_AS(keet::time::sleepSec(-0.5), keet::error);

        try {
            keet::time::sleep(-5);
        } catch (const keet::error& err) {
            REQUIRE(err.name == "TimeError");
        }
    }

    SECTION("NaN and Inf in sleepSec throw TimeError") {
        REQUIRE_THROWS_AS(keet::time::sleepSec(std::numeric_limits<double>::quiet_NaN()), keet::error);
        REQUIRE_THROWS_AS(keet::time::sleepSec(std::numeric_limits<double>::infinity()), keet::error);

        try {
            keet::time::sleepSec(std::numeric_limits<double>::quiet_NaN());
        } catch (const keet::error& err) {
            REQUIRE(err.name == "TimeError");
        }
    }
}

TEST_CASE("keet::time formatting and calendar component extraction", "[runtime][time]") {
    SECTION("Epoch 0 (1970-01-01T00:00:00Z)") {
        REQUIRE(keet::time::formatIso8601(0) == "1970-01-01T00:00:00Z");
        REQUIRE(keet::time::yearUtc(0) == 1970);
        REQUIRE(keet::time::monthUtc(0) == 1);
        REQUIRE(keet::time::dayUtc(0) == 1);
        REQUIRE(keet::time::hourUtc(0) == 0);
        REQUIRE(keet::time::minuteUtc(0) == 0);
        REQUIRE(keet::time::secondUtc(0) == 0);
    }

    SECTION("Known epoch timestamp 1700000000000 ms (2023-11-14T22:13:20Z)") {
        int64_t ts = 1700000000000LL;
        REQUIRE(keet::time::formatIso8601(ts) == "2023-11-14T22:13:20Z");
        REQUIRE(keet::time::yearUtc(ts) == 2023);
        REQUIRE(keet::time::monthUtc(ts) == 11);
        REQUIRE(keet::time::dayUtc(ts) == 14);
        REQUIRE(keet::time::hourUtc(ts) == 22);
        REQUIRE(keet::time::minuteUtc(ts) == 13);
        REQUIRE(keet::time::secondUtc(ts) == 20);

        REQUIRE(keet::time::formatUtc(ts, "%Y-%m-%d %H:%M:%S") == "2023-11-14 22:13:20");
        REQUIRE(keet::time::formatUtc(ts, "%Y") == "2023");
    }

    SECTION("formatLocal produces valid non-empty output") {
        int64_t ts = 1700000000000LL;
        std::string local_yr = keet::time::formatLocal(ts, "%Y");
        REQUIRE((local_yr == "2023" || local_yr == "2024"));
    }

    SECTION("Empty pattern throws TimeError") {
        REQUIRE_THROWS_AS(keet::time::formatUtc(0, ""), keet::error);
        REQUIRE_THROWS_AS(keet::time::formatLocal(0, ""), keet::error);

        try {
            (void)keet::time::formatUtc(0, "");
        } catch (const keet::error& err) {
            REQUIRE(err.name == "TimeError");
        }
    }
}
