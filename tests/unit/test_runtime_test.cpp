#include <catch2/catch_test_macros.hpp>
#include <keet/test.hpp>
#include <keet/error.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

TEST_CASE("keet::test assert_eq primitives", "[runtime][test]") {
    // Integer equality
    REQUIRE_NOTHROW(keet::test::assert_eq(42, 42));
    REQUIRE_THROWS_AS(keet::test::assert_eq(42, 43), keet::test::AssertionError);

    // String equality
    REQUIRE_NOTHROW(keet::test::assert_eq(std::string("hello"), std::string("hello")));
    REQUIRE_THROWS_AS(keet::test::assert_eq(std::string("hello"), std::string("world")), keet::test::AssertionError);

    // Boolean equality
    REQUIRE_NOTHROW(keet::test::assert_eq(true, true));
    REQUIRE_NOTHROW(keet::test::assert_eq(false, false));
    REQUIRE_THROWS_AS(keet::test::assert_eq(true, false), keet::test::AssertionError);

    // Floating point equality
    REQUIRE_NOTHROW(keet::test::assert_eq(3.14, 3.14));
    REQUIRE_THROWS_AS(keet::test::assert_eq(3.14, 2.71), keet::test::AssertionError);
}

TEST_CASE("keet::test assert_eq collections", "[runtime][test]") {
    // Vector equality
    std::vector<int64_t> v1 = {1, 2, 3};
    std::vector<int64_t> v2 = {1, 2, 3};
    std::vector<int64_t> v3 = {1, 2, 4};
    REQUIRE_NOTHROW(keet::test::assert_eq(v1, v2));
    REQUIRE_THROWS_AS(keet::test::assert_eq(v1, v3), keet::test::AssertionError);

    // Map equality
    std::unordered_map<std::string, int64_t> m1 = {{"a", 1}, {"b", 2}};
    std::unordered_map<std::string, int64_t> m2 = {{"a", 1}, {"b", 2}};
    std::unordered_map<std::string, int64_t> m3 = {{"a", 1}, {"b", 3}};
    REQUIRE_NOTHROW(keet::test::assert_eq(m1, m2));
    REQUIRE_THROWS_AS(keet::test::assert_eq(m1, m3), keet::test::AssertionError);
}

TEST_CASE("keet::test assert_ne", "[runtime][test]") {
    REQUIRE_NOTHROW(keet::test::assert_ne(10, 20));
    REQUIRE_NOTHROW(keet::test::assert_ne(std::string("abc"), std::string("def")));
    REQUIRE_THROWS_AS(keet::test::assert_ne(10, 10), keet::test::AssertionError);
    REQUIRE_THROWS_AS(keet::test::assert_ne(std::string("same"), std::string("same")), keet::test::AssertionError);
}

TEST_CASE("keet::test assert_true and assert_false", "[runtime][test]") {
    REQUIRE_NOTHROW(keet::test::assert_true(true));
    REQUIRE_NOTHROW(keet::test::assert_true(10 > 5));
    REQUIRE_THROWS_AS(keet::test::assert_true(false), keet::test::AssertionError);
    REQUIRE_THROWS_AS(keet::test::assert_true(5 > 10), keet::test::AssertionError);

    REQUIRE_NOTHROW(keet::test::assert_false(false));
    REQUIRE_NOTHROW(keet::test::assert_false(5 > 10));
    REQUIRE_THROWS_AS(keet::test::assert_false(true), keet::test::AssertionError);
    REQUIRE_THROWS_AS(keet::test::assert_false(10 > 5), keet::test::AssertionError);
}

TEST_CASE("keet::test assert_zonks error verification", "[runtime][test]") {
    // Case 1: Throws expected keet::error
    auto throwDivZero = []() {
        throw keet::error("DivisionByZero", "Cannot divide by zero");
    };
    REQUIRE_NOTHROW(keet::test::assert_zonks(throwDivZero));
    REQUIRE_NOTHROW(keet::test::assert_zonks(throwDivZero, "DivisionByZero"));

    // Case 2: Throws different error name
    REQUIRE_THROWS_AS(keet::test::assert_zonks(throwDivZero, "IndexOutOfBounds"), keet::test::AssertionError);

    // Case 3: Does not throw -> assertion fails
    auto noThrow = []() {
        int x = 1 + 1;
        (void)x;
    };
    REQUIRE_THROWS_AS(keet::test::assert_zonks(noThrow), keet::test::AssertionError);
    REQUIRE_THROWS_AS(keet::test::assert_zonks(noThrow, "DivisionByZero"), keet::test::AssertionError);
}

TEST_CASE("keet::test fail explicitly", "[runtime][test]") {
    REQUIRE_THROWS_AS(keet::test::fail("Custom explicit failure"), keet::test::AssertionError);
    try {
        keet::test::fail("Test failed message");
    } catch (const keet::test::AssertionError& err) {
        REQUIRE(err.name == "AssertionError");
        REQUIRE(err.message == "Test failed message");
    }
}

TEST_CASE("keet::test Registry lifecycle and suite hierarchy", "[runtime][test]") {
    auto& reg = keet::test::Registry::instance();
    reg.clear();
    REQUIRE(reg.cases().empty());

    // Register test cases via user-facing namespace
    keet_module_test::_keet_suite("Math Suite", [&]() {
        keet_module_test::_keet_case("addition", []() {
            keet_module_test::assertEq(2 + 2, 4);
        });

        keet_module_test::_keet_case("subtraction", []() {
            keet_module_test::assertEq(5 - 3, 2);
        });
    });

    keet_module_test::_keet_suite("Error Suite", [&]() {
        keet_module_test::_keet_case("expected error", []() {
            keet_module_test::assertZonks([]() {
                throw keet::error("CustomError", "Failed");
            }, "CustomError");
        });

        keet_module_test::_keet_case("intentional failure", []() {
            keet_module_test::assertEq(1, 2, "intentional mismatch");
        });
    });

    REQUIRE(reg.cases().size() == 4);

    // Run all tests
    auto summary = reg.runAll();
    REQUIRE(summary.total == 4);
    REQUIRE(summary.passed == 3);
    REQUIRE(summary.failed == 1);
    REQUIRE(summary.totalDurationNs > 0);

    // Verify individual results
    REQUIRE(summary.results[0].name == "addition");
    REQUIRE(summary.results[0].suite == "Math Suite");
    REQUIRE(summary.results[0].passed == true);

    REQUIRE(summary.results[3].name == "intentional failure");
    REQUIRE(summary.results[3].suite == "Error Suite");
    REQUIRE(summary.results[3].passed == false);
    REQUIRE(summary.results[3].failure.has_value());
    REQUIRE(summary.results[3].failure->expected == "2");
    REQUIRE(summary.results[3].failure->actual == "1");

    // Clean up
    reg.clear();
}

TEST_CASE("keet::test Registry filtering and json serialization", "[runtime][test]") {
    auto& reg = keet::test::Registry::instance();
    reg.clear();

    keet_module_test::_keet_suite("Alpha", [&]() {
        keet_module_test::_keet_case("test_one", []() {});
        keet_module_test::_keet_case("test_two", []() {});
    });

    keet_module_test::_keet_suite("Beta", [&]() {
        keet_module_test::_keet_case("test_three", []() {});
    });

    // Filter for "Alpha"
    auto summaryAlpha = reg.runAll("Alpha");
    REQUIRE(summaryAlpha.total == 2);
    REQUIRE(summaryAlpha.passed == 2);
    REQUIRE(summaryAlpha.skipped == 1);

    // Filter for "three"
    auto summaryThree = reg.runAll("three");
    REQUIRE(summaryThree.total == 1);
    REQUIRE(summaryThree.passed == 1);
    REQUIRE(summaryThree.skipped == 2);

    // Test JSON serialization
    std::string jsonStr = keet::test::summaryToJson(summaryThree);
    REQUIRE(jsonStr.find("\"total\": 1") != std::string::npos);
    REQUIRE(jsonStr.find("\"passed\": 1") != std::string::npos);
    REQUIRE(jsonStr.find("\"name\": \"test_three\"") != std::string::npos);

    // Test text summary formatting
    std::ostringstream out;
    keet::test::printSummary(summaryAlpha, out, false);
    std::string textOut = out.str();
    REQUIRE(textOut.find("Alpha") != std::string::npos);
    REQUIRE(textOut.find("test_one") != std::string::npos);
    REQUIRE(textOut.find("2 passed") != std::string::npos);

    reg.clear();
}
