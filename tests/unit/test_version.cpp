#include <catch2/catch_test_macros.hpp>
#include <keety/version.hpp>

TEST_CASE("Compiler version check", "[version]") {
    REQUIRE(keety::getVersionString() == "0.1.0");
    REQUIRE_FALSE(keety::getVersionString().empty());
}
