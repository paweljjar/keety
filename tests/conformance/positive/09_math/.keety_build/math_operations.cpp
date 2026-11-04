// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "math_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_math_operations {
#line 5 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
    int64_t _keet_main() {
#line 6 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        ::keet::io::println("Running KeetScript math conformance tests...");
#line 9 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::PI < 3.14 || ::keet::math::PI > 3.15) {
#line 10 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 1;
        }
#line 12 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::E < 2.71 || ::keet::math::E > 2.72) {
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 2;
        }
#line 15 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::TAU < 6.28 || ::keet::math::TAU > 6.29) {
#line 16 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 3;
        }
#line 18 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (!::keet::math::isInf(::keet::math::INF)) {
#line 19 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 4;
        }
#line 21 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (!::keet::math::isNan(::keet::math::NAN)) {
#line 22 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 5;
        }
#line 26 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double sin_pi = ::keet::math::sin(::keet::math::PI);
#line 27 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::abs(sin_pi) > 0.0001) {
#line 28 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 6;
        }
#line 30 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::abs(::keet::math::cos(0.0) - 1.0) > 0.0001) {
#line 31 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 7;
        }
#line 33 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::abs(::keet::math::toRadians(180.0) - ::keet::math::PI) > 0.0001) {
#line 34 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 8;
        }
#line 36 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::abs(::keet::math::toDegrees(::keet::math::PI) - 180.0) > 0.0001) {
#line 37 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 9;
        }
#line 41 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::absInt(-42) != 42) {
#line 42 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 10;
        }
#line 44 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::minInt(10, 20) != 10) {
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 11;
        }
#line 47 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::maxInt(10, 20) != 20) {
#line 48 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 12;
        }
#line 50 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::clampInt(5, 0, 10) != 5) {
#line 51 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 13;
        }
#line 53 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::clampInt(-5, 0, 10) != 0) {
#line 54 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 14;
        }
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::clampInt(15, 0, 10) != 10) {
#line 57 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 15;
        }
#line 59 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::clamp(1.5, 0.0, 2.0) != 1.5) {
#line 60 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 16;
        }
#line 64 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::floor(3.7) != 3.0) {
#line 65 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 17;
        }
#line 67 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::ceil(3.2) != 4.0) {
#line 68 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 18;
        }
#line 70 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::round(3.5) != 4.0) {
#line 71 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 19;
        }
#line 73 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::trunc(-3.9) != -3.0) {
#line 74 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 20;
        }
#line 76 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::sqrt(16.0) != 4.0) {
#line 77 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 21;
        }
#line 79 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::pow(2.0, 3.0) != 8.0) {
#line 80 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 22;
        }
#line 82 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::hypot(3.0, 4.0) != 5.0) {
#line 83 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 23;
        }
#line 85 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (::keet::math::lerp(10.0, 20.0, 0.5) != 15.0) {
#line 86 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 24;
        }
#line 90 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        ::keet::math::seed(1337);
#line 91 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double r1 = ::keet::math::random();
#line 92 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        int64_t i1 = ::keet::math::randomInt(1, 100);
#line 93 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double f1 = ::keet::math::randomFloat(0.0, 10.0);
#line 95 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (r1 < 0.0 || r1 >= 1.0) {
#line 96 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 25;
        }
#line 98 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (i1 < 1 || i1 > 100) {
#line 99 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 26;
        }
#line 101 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (f1 < 0.0 || f1 >= 10.0) {
#line 102 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 27;
        }
#line 105 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        ::keet::math::seed(1337);
#line 106 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double r2 = ::keet::math::random();
#line 107 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        int64_t i2 = ::keet::math::randomInt(1, 100);
#line 108 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double f2 = ::keet::math::randomFloat(0.0, 10.0);
#line 110 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (r1 != r2 || i1 != i2 || f1 != f2) {
#line 111 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 28;
        }
#line 115 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        double safe_val = ([&]() -> std::decay_t<decltype(::keet::math::clamp(5.0, 10.0, 0.0))> { try { return ::keet::math::clamp(5.0, 10.0, 0.0); } catch (...) { return 99.0; } })();
#line 116 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (safe_val != 99.0) {
#line 117 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 29;
        }
#line 120 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        int64_t safe_int = ([&]() -> std::decay_t<decltype(::keet::math::clampInt(5, 10, 0))> { try { return ::keet::math::clampInt(5, 10, 0); } catch (...) { return 99; } })();
#line 121 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        if (safe_int != 99) {
#line 122 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
            return 30;
        }
#line 125 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        ::keet::io::println("KeetScript math conformance tests passed successfully!");
#line 126 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/09_math/math_operations.keet"
        return 0;
    }
} // namespace keet_module_math_operations

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_math_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
