// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_ternary_narrowing.hpp"
#include <iostream>

namespace keet_module_union_ternary_narrowing {
    int64_t test_fallback(const std::variant<int64_t, std::monostate>& a) {
        int64_t b = ((a == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(std::get<int64_t>(a)));
        return b;
    }

    int64_t test_narrowed(const std::variant<int64_t, std::monostate>& a) {
        int64_t c = ((a != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(std::get<int64_t>(a)) : keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(5));
        return c;
    }

    int64_t _keet_main() {
        std::variant<int64_t, std::monostate> a = std::monostate{};
        int64_t b = ((a == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(a))>(std::get<int64_t>(a)));
        int64_t c = ((a != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(std::get<int64_t>(a)) : keet::branch_type_t<decltype(std::get<int64_t>(a)), decltype(5)>(5));
        if (b != 5) {
            return 1;
        }
        if (c != 5) {
            return 2;
        }
        std::variant<int64_t, std::monostate> x = 42;
        int64_t y = ((x == std::monostate{}) ? keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(x))>(5) : keet::branch_type_t<decltype(5), decltype(std::get<int64_t>(x))>(std::get<int64_t>(x)));
        int64_t z = ((x != std::monostate{}) ? keet::branch_type_t<decltype(std::get<int64_t>(x)), decltype(5)>(std::get<int64_t>(x)) : keet::branch_type_t<decltype(std::get<int64_t>(x)), decltype(5)>(5));
        if (y != 42) {
            return 3;
        }
        if (z != 42) {
            return 4;
        }
        if (test_fallback(std::monostate{}) != 5) {
            return 5;
        }
        if (test_fallback(100) != 100) {
            return 6;
        }
        if (test_narrowed(std::monostate{}) != 5) {
            return 7;
        }
        if (test_narrowed(100) != 100) {
            return 8;
        }
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
