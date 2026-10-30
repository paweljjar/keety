#include <catch2/catch_test_macros.hpp>
#include <keet/bool_byte.hpp>
#include <vector>

TEST_CASE("keet::BoolByte size and conversions", "[runtime]") {
    STATIC_REQUIRE(sizeof(keet::BoolByte) == 1);

    keet::BoolByte bDefault;
    REQUIRE_FALSE(static_cast<bool>(bDefault));

    keet::BoolByte bTrue(true);
    REQUIRE(static_cast<bool>(bTrue));

    keet::BoolByte bFalse(false);
    REQUIRE_FALSE(static_cast<bool>(bFalse));

    bDefault = true;
    REQUIRE(static_cast<bool>(bDefault));
}

TEST_CASE("keet::BoolByte comparisons and conditional usage", "[runtime]") {
    keet::BoolByte b1(true);
    keet::BoolByte b2(true);
    keet::BoolByte b3(false);

    REQUIRE(b1 == b2);
    REQUIRE(b1 != b3);
    REQUIRE(b1 == true);
    REQUIRE(b3 == false);

    if (b1) {
        REQUIRE(true);
    } else {
        FAIL("b1 should evaluate to true");
    }

    if (!b3) {
        REQUIRE(true);
    } else {
        FAIL("!b3 should evaluate to true");
    }
}

TEST_CASE("keet::BoolByte in std::vector provides addressable l-values", "[runtime]") {
    // std::vector<keet::BoolByte> does NOT specialize like std::vector<bool>,
    // allowing contiguous memory and direct l-value reference binding:
    std::vector<keet::BoolByte> vec = {true, false, true};
    REQUIRE(vec.size() == 3);

    // Direct mutable reference without proxy objects:
    keet::BoolByte& first = vec[0];
    first = false;
    REQUIRE_FALSE(vec[0]);

    // Data pointer is standard contiguous array of bytes:
    keet::BoolByte* dataPtr = vec.data();
    REQUIRE(dataPtr != nullptr);
    REQUIRE(dataPtr[1] == false);
}
