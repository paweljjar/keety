// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "scalars_divzero_recover.hpp"
#include <iostream>

namespace keet_module_scalars_divzero_recover {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
    int64_t safeDiv(int64_t a, int64_t b) {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
        return ([&]() -> std::decay_t<decltype(keet::div_safe(a, b))> { try { return keet::div_safe(a, b); } catch (...) { return -1; } })();
    }

#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
    int64_t _keet_main() {
#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
        if (safeDiv(10, 2) != 5) {
#line 7 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
            return 1;
        }
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
        if (safeDiv(10, 0) != -1) {
#line 10 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
            return 2;
        }
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/01_scalars/scalars_divzero_recover.keet"
        return 0;
    }
} // namespace keet_module_scalars_divzero_recover

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_scalars_divzero_recover::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
