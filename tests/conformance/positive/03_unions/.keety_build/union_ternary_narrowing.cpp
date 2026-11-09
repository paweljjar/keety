// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_ternary_narrowing.hpp"
#include <iostream>

namespace keet_module_union_ternary_narrowing {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
    int64_t test_fallback(const std::variant<int64_t, std::monostate>& a) {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t b = ((a == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(std::get<int64_t>(a)));
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        return b;
    }

#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
    int64_t test_narrowed(const std::variant<int64_t, std::monostate>& a) {
#line 7 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t c = ((a != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(std::get<int64_t>(a)) : keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(5));
#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        return c;
    }

#line 11 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
    int64_t _keet_main() {
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        std::variant<int64_t, std::monostate> a = std::monostate{};
#line 13 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t b = ((a == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(std::get<int64_t>(a)));
#line 14 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t c = ((a != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(std::get<int64_t>(a)) : keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(5));
#line 16 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (b != 5) {
#line 17 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 1;
        }
#line 19 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (c != 5) {
#line 20 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 2;
        }
#line 23 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        std::variant<int64_t, std::monostate> x = 42;
#line 24 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t y = ((x == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(x))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(x))>(std::get<int64_t>(x)));
#line 25 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        int64_t z = ((x != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(x)), decltype(5)>(std::get<int64_t>(x)) : keet::branch_type_t<decltype(std::get<int64_t>(x)), decltype(5)>(5));
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (y != 42) {
#line 28 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 3;
        }
#line 30 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (z != 42) {
#line 31 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 4;
        }
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (test_fallback(std::monostate{}) != 5) {
#line 35 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 5;
        }
#line 37 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (test_fallback(100) != 100) {
#line 38 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 6;
        }
#line 40 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (test_narrowed(std::monostate{}) != 5) {
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 7;
        }
#line 43 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        if (test_narrowed(100) != 100) {
#line 44 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
            return 8;
        }
#line 47 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_ternary_narrowing.keet"
        return 0;
    }
} // namespace keet_module_union_ternary_narrowing

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_union_ternary_narrowing::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
