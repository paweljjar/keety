// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_flow_narrowing.hpp"
#include <iostream>

namespace keet_module_union_flow_narrowing {
    std::string formatValue(const std::variant<int64_t, std::string, std::monostate>& v) {
        if (keet::type_of(v) == "int") {
            int64_t i = std::get<int64_t>(v);
            if (i == 42) {
                return "int: forty-two";
            }
            return "int: other";
        } else if (keet::type_of(v) == "string") {
            std::string s = std::get<std::string>(v);
            return "str: " + s;
        } else {
            return "empty";
        }
    }

    int64_t getOrDefault(const std::variant<int64_t, std::monostate>& v, int64_t def) {
        if (v != std::monostate{}) {
            return std::get<int64_t>(v);
        }
        return def;
    }

    bool isPositiveInt(const std::variant<int64_t, std::string>& v) {
        if (keet::type_of(v) == "int") {
            int64_t i = std::get<int64_t>(v);
            if (i > 0) {
                return true;
            }
        }
        return false;
    }

    int64_t _keet_main() {
        std::string r1 = formatValue(42);
        if (r1 != "int: forty-two") {
            return 1;
        }
        std::string r2 = formatValue(100);
        if (r2 != "int: other") {
            return 2;
        }
        std::string r3 = formatValue("hello");
        if (r3 != "str: hello") {
            return 3;
        }
        std::string r4 = formatValue(std::monostate{});
        if (r4 != "empty") {
            return 4;
        }
        std::variant<int64_t, std::monostate> opt1 = 55;
        if (getOrDefault(opt1, 10) != 55) {
            return 5;
        }
        std::variant<int64_t, std::monostate> opt2 = std::monostate{};
        if (getOrDefault(opt2, 10) != 10) {
            return 6;
        }
        if (!isPositiveInt(15)) {
            return 7;
        }
        if (isPositiveInt(-8)) {
            return 8;
        }
        if (isPositiveInt("test")) {
            return 9;
        }
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
