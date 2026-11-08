// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_flow_narrowing.hpp"
#include <iostream>

namespace keet_module_union_flow_narrowing {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
    std::string formatValue(const std::variant<int64_t, std::string, std::monostate>& v) {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (keet::type_of(v) == "int") {
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            int64_t i = std::get<int64_t>(v);
#line 4 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            if (i == 42) {
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
                return "int: forty-two";
            }
#line 7 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return "int: other";
        }
#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        else if (keet::type_of(v) == "string") {
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            std::string s = std::get<std::string>(v);
#line 10 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return "str: " + s;
        } else {
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return "empty";
        }
    }

#line 16 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
    int64_t getOrDefault(const std::variant<int64_t, std::monostate>& v, int64_t def) {
#line 17 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (v != std::monostate{}) {
#line 18 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return std::get<int64_t>(v);
        }
#line 20 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        return def;
    }

#line 23 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
    bool isPositiveInt(const std::variant<int64_t, std::string>& v) {
#line 24 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (keet::type_of(v) == "int") {
#line 25 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            int64_t i = std::get<int64_t>(v);
#line 26 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            if (i > 0) {
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
                return true;
            }
        }
#line 30 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        return false;
    }

#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
    int64_t _keet_main() {
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::string r1 = formatValue(42);
#line 35 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (r1 != "int: forty-two") {
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 1;
        }
#line 39 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::string r2 = formatValue(100);
#line 40 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (r2 != "int: other") {
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 2;
        }
#line 44 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::string r3 = formatValue("hello");
#line 45 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (r3 != "str: hello") {
#line 46 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 3;
        }
#line 49 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::string r4 = formatValue(std::monostate{});
#line 50 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (r4 != "empty") {
#line 51 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 4;
        }
#line 54 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::variant<int64_t, std::monostate> opt1 = 55;
#line 55 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (getOrDefault(opt1, 10) != 55) {
#line 56 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 5;
        }
#line 59 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        std::variant<int64_t, std::monostate> opt2 = std::monostate{};
#line 60 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (getOrDefault(opt2, 10) != 10) {
#line 61 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 6;
        }
#line 64 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (!isPositiveInt(15)) {
#line 65 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 7;
        }
#line 68 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (isPositiveInt(-8)) {
#line 69 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 8;
        }
#line 72 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        if (isPositiveInt("test")) {
#line 73 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
            return 9;
        }
#line 76 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/03_unions/union_flow_narrowing.keet"
        return 0;
    }
} // namespace keet_module_union_flow_narrowing

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_union_flow_narrowing::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
