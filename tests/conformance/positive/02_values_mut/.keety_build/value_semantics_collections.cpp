// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "value_semantics_collections.hpp"
#include <iostream>

namespace keet_module_value_semantics_collections {
    int64_t _keet_main() {
        std::vector<int64_t> a = {1, 2, 3};
        std::vector<int64_t> b = a;
        b.push_back(4);
        if (a.size() != 3) {
            return 1;
        }
        if (b.size() != 4) {
            return 2;
        }
        return 0;
    }
} // namespace keet_module_value_semantics_collections

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_value_semantics_collections::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
