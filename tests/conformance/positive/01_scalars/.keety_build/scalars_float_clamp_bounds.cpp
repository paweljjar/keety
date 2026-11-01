// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "scalars_float_clamp_bounds.hpp"
#include <iostream>

namespace keet_module_scalars_float_clamp_bounds {
    int64_t _keet_main() {
        int64_t overflowInt = ([&]() -> std::decay_t<decltype(keet::to_int(1e25))> { try { return keet::to_int(1e25); } catch (...) { return -999; } })();
        int64_t nanInt = ([&]() -> std::decay_t<decltype(keet::to_int(keet::div_safe(0.0, 0.0)))> { try { return keet::to_int(keet::div_safe(0.0, 0.0)); } catch (...) { return -111; } })();
        double clamped = keet::clamp(15.5, 0.0, 10.0);
        if (overflowInt != -999) {
            return 1;
        }
        if (nanInt != -111) {
            return 2;
        }
        if (clamped != 10.0) {
            return 3;
        }
        return 0;
    }
} // namespace keet_module_scalars_float_clamp_bounds

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_scalars_float_clamp_bounds::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
