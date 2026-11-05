// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "value_semantics_collections.hpp"
#include <iostream>

namespace keet_module_value_semantics_collections {
#line 1 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
    int64_t _keet_main() {
#line 2 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        std::vector<int64_t> a = {1, 2, 3};
#line 3 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        std::vector<int64_t> b = a;
#line 4 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        b.push_back(4);
#line 6 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        if (a.size() != 3) {
#line 7 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
            return 1;
        }
#line 9 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        if (b.size() != 4) {
#line 10 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
            return 2;
        }
#line 12 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/02_values_mut/value_semantics_collections.keet"
        return 0;
    }
} // namespace keet_module_value_semantics_collections

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_value_semantics_collections::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
