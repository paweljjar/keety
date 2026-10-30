#include <catch2/catch_test_macros.hpp>
#include <keet/numeric.hpp>
#include <limits>

TEST_CASE("Deterministic two's-complement overflow operations (Test 1.1 Conformance)", "[runtime]") {
    constexpr int64_t maxInt = std::numeric_limits<int64_t>::max();  // 9223372036854775807
    constexpr int64_t minInt = std::numeric_limits<int64_t>::min();  // -9223372036854775808

    // Test 1.1: maxInt + 1 == minInt
    int64_t wrapped = keet::add_wrap(maxInt, 1);
    REQUIRE(wrapped == minInt);

    // minInt - 1 == maxInt
    int64_t wrappedUnder = keet::sub_wrap(minInt, 1);
    REQUIRE(wrappedUnder == maxInt);

    // -minInt == minInt (two's complement wraparound)
    int64_t negatedMin = keet::neg_wrap(minInt);
    REQUIRE(negatedMin == minInt);

    // Multiplication overflow wraparound
    int64_t mulWrapped = keet::mul_wrap(maxInt, 2);
    REQUIRE(mulWrapped == -2);
}

TEST_CASE("Safe division and modulo with error raising (Test 1.2 Conformance)", "[runtime]") {
    // Normal cases
    REQUIRE(keet::div_safe(10, 2) == 5);
    REQUIRE(keet::div_safe(-10, 2) == -5);
    REQUIRE(keet::mod_safe(10, 3) == 1);

    // Division by zero throws keet::error("DivisionByZero")
    REQUIRE_THROWS_AS(keet::div_safe(10, 0), keet::error);
    REQUIRE_THROWS_AS(keet::mod_safe(10, 0), keet::error);

    try {
        (void)keet::div_safe(42, 0);
        FAIL("Expected DivisionByZero error");
    } catch (const keet::error& err) {
        REQUIRE(err.name == "DivisionByZero");
    }

    // INT64_MIN / -1 wraparound
    constexpr int64_t minInt = std::numeric_limits<int64_t>::min();
    REQUIRE(keet::div_safe(minInt, -1) == minInt);
    REQUIRE(keet::mod_safe(minInt, -1) == 0);
}
