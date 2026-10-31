// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "scalars_divzero_recover.hpp"
#include <iostream>

namespace keet_module_scalars_divzero_recover {
    int64_t safeDiv(int64_t a, int64_t b) {
        return ([&]() -> std::decay_t<decltype(keet::div_safe(a, b))> { try { return keet::div_safe(a, b); } catch (...) { return -1; } })();
    }

    int64_t _keet_main() {
        if (safeDiv(10, 2) != 5) {
            return 1;
        }
        if (safeDiv(10, 0) != -1) {
            return 2;
        }
        return 0;
    }
} // namespace keet_module_scalars_divzero_recover

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_scalars_divzero_recover::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
