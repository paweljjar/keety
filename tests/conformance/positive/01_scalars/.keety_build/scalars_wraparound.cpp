// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "scalars_wraparound.hpp"
#include <iostream>

namespace keet_module_scalars_wraparound {
#line 1 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
    int64_t _keet_main() {
#line 2 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        int64_t maxInt = 9223372036854775807LL;
#line 3 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        int64_t wrapped = maxInt + 1;
#line 4 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        int64_t minInt = (-9223372036854775807LL - 1LL);
#line 5 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        int64_t negatedMin = -minInt;
#line 7 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        if (wrapped != (-9223372036854775807LL - 1LL)) {
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
            return 1;
        }
#line 10 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        if (negatedMin != (-9223372036854775807LL - 1LL)) {
#line 11 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
            return 2;
        }
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/01_scalars/scalars_wraparound.keet"
        return 0;
    }
} // namespace keet_module_scalars_wraparound

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_scalars_wraparound::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
