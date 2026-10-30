#include <catch2/catch_test_macros.hpp>
#include <keet/error.hpp>

TEST_CASE("keet::error default construction and properties", "[runtime]") {
    keet::error err;
    REQUIRE(err.name == "Error");
    REQUIRE(err.message == "[Error]");
    REQUIRE(err.toString() == "[Error]: [Error]");
    REQUIRE(std::string(err.what()) == "[Error]");
}

TEST_CASE("keet::error custom construction and polymorphic catching", "[runtime]") {
    keet::error customErr("DivisionByZero", "Cannot divide by zero");
    REQUIRE(customErr.name == "DivisionByZero");
    REQUIRE(customErr.message == "Cannot divide by zero");
    REQUIRE(customErr.toString() == "[DivisionByZero]: Cannot divide by zero");
    REQUIRE(std::string(customErr.what()) == "Cannot divide by zero");

    // Verify polymorphic catch as std::exception
    try {
        throw customErr;
    } catch (const std::exception& ex) {
        REQUIRE(std::string(ex.what()) == "Cannot divide by zero");
    }
}
