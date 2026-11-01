// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "math_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_math_operations {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript math conformance tests...");
        if (::keet::math::PI < 3.14 || ::keet::math::PI > 3.15) {
            return 1;
        }
        if (::keet::math::E < 2.71 || ::keet::math::E > 2.72) {
            return 2;
        }
        if (::keet::math::TAU < 6.28 || ::keet::math::TAU > 6.29) {
            return 3;
        }
        if (!::keet::math::isInf(::keet::math::INF)) {
            return 4;
        }
        if (!::keet::math::isNan(::keet::math::NAN)) {
            return 5;
        }
        double sin_pi = ::keet::math::sin(::keet::math::PI);
        if (::keet::math::abs(sin_pi) > 0.0001) {
            return 6;
        }
        if (::keet::math::abs(::keet::math::cos(0.0) - 1.0) > 0.0001) {
            return 7;
        }
        if (::keet::math::abs(::keet::math::toRadians(180.0) - ::keet::math::PI) > 0.0001) {
            return 8;
        }
        if (::keet::math::abs(::keet::math::toDegrees(::keet::math::PI) - 180.0) > 0.0001) {
            return 9;
        }
        if (::keet::math::absInt(-42) != 42) {
            return 10;
        }
        if (::keet::math::minInt(10, 20) != 10) {
            return 11;
        }
        if (::keet::math::maxInt(10, 20) != 20) {
            return 12;
        }
        if (::keet::math::clampInt(5, 0, 10) != 5) {
            return 13;
        }
        if (::keet::math::clampInt(-5, 0, 10) != 0) {
            return 14;
        }
        if (::keet::math::clampInt(15, 0, 10) != 10) {
            return 15;
        }
        if (::keet::math::clamp(1.5, 0.0, 2.0) != 1.5) {
            return 16;
        }
        if (::keet::math::floor(3.7) != 3.0) {
            return 17;
        }
        if (::keet::math::ceil(3.2) != 4.0) {
            return 18;
        }
        if (::keet::math::round(3.5) != 4.0) {
            return 19;
        }
        if (::keet::math::trunc(-3.9) != -3.0) {
            return 20;
        }
        if (::keet::math::sqrt(16.0) != 4.0) {
            return 21;
        }
        if (::keet::math::pow(2.0, 3.0) != 8.0) {
            return 22;
        }
        if (::keet::math::hypot(3.0, 4.0) != 5.0) {
            return 23;
        }
        if (::keet::math::lerp(10.0, 20.0, 0.5) != 15.0) {
            return 24;
        }
        ::keet::math::seed(1337);
        double r1 = ::keet::math::random();
        int64_t i1 = ::keet::math::randomInt(1, 100);
        double f1 = ::keet::math::randomFloat(0.0, 10.0);
        if (r1 < 0.0 || r1 >= 1.0) {
            return 25;
        }
        if (i1 < 1 || i1 > 100) {
            return 26;
        }
        if (f1 < 0.0 || f1 >= 10.0) {
            return 27;
        }
        ::keet::math::seed(1337);
        double r2 = ::keet::math::random();
        int64_t i2 = ::keet::math::randomInt(1, 100);
        double f2 = ::keet::math::randomFloat(0.0, 10.0);
        if (r1 != r2 || i1 != i2 || f1 != f2) {
            return 28;
        }
        double safe_val = ([&]() -> std::decay_t<decltype(::keet::math::clamp(5.0, 10.0, 0.0))> { try { return ::keet::math::clamp(5.0, 10.0, 0.0); } catch (...) { return 99.0; } })();
        if (safe_val != 99.0) {
            return 29;
        }
        int64_t safe_int = ([&]() -> std::decay_t<decltype(::keet::math::clampInt(5, 10, 0))> { try { return ::keet::math::clampInt(5, 10, 0); } catch (...) { return 99; } })();
        if (safe_int != 99) {
            return 30;
        }
        ::keet::io::println("KeetScript math conformance tests passed successfully!");
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
