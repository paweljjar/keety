// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "mut_parameter_swap.hpp"
#include <iostream>

namespace keet_module_mut_parameter_swap {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
    template <typename T>
    void swap(T& x, T& y) {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        T temp = x;
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        x = y;
#line 4 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        y = temp;
    }

#line 7 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
    int64_t _keet_main() {
#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        int64_t u = 10;
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        int64_t v = 20;
#line 10 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        swap(u, v);
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        if (u != 20) {
#line 13 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
            return 1;
        }
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        if (v != 10) {
#line 16 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
            return 2;
        }
#line 18 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/02_values_mut/mut_parameter_swap.keet"
        return 0;
    }
} // namespace keet_module_mut_parameter_swap

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_mut_parameter_swap::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
