// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "mut_parameter_swap.hpp"
#include <iostream>

namespace keet_module_mut_parameter_swap {
    template <typename T>
    void swap(T& x, T& y) {
        T temp = x;
        x = y;
        y = temp;
    }

    int64_t _keet_main() {
        int64_t u = 10;
        int64_t v = 20;
        swap(u, v);
        if (u != 20) {
            return 1;
        }
        if (v != 10) {
            return 2;
        }
        return 0;
    }
} // namespace keet_module_mut_parameter_swap

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_mut_parameter_swap::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
