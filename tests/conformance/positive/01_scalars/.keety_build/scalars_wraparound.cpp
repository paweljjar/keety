// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "scalars_wraparound.hpp"
#include <iostream>

namespace keet_module_scalars_wraparound {
    int64_t _keet_main() {
        int64_t maxInt = 9223372036854775807LL;
        int64_t wrapped = maxInt + 1;
        int64_t minInt = (-9223372036854775807LL - 1LL);
        int64_t negatedMin = -minInt;
        if (wrapped != (-9223372036854775807LL - 1LL)) {
            return 1;
        }
        if (negatedMin != (-9223372036854775807LL - 1LL)) {
            return 2;
        }
        return 0;
    }
} // namespace keet_module_scalars_wraparound

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_scalars_wraparound::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
