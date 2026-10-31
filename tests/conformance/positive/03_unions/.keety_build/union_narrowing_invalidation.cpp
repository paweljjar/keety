// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_narrowing_invalidation.hpp"
#include <iostream>

namespace keet_module_union_narrowing_invalidation {
    int64_t mutateAndVerify() {
        std::variant<int64_t, std::string, std::monostate> val = 10;
        if (keet::type_of(val) == "int") {
            int64_t initial = std::get<int64_t>(val);
            if (initial != 10) {
                return 1;
            }
            val = "hello";
        }
        if (keet::type_of(val) == "string") {
            std::string text = std::get<std::string>(val);
            if (text != "hello") {
                return 2;
            }
            val = std::monostate{};
        }
        if (val == std::monostate{}) {
            return 0;
        }
        return 3;
    }

    int64_t testGuardedReassignment() {
        std::variant<int64_t, std::string> item = 100;
        int64_t state = 0;
        if (keet::type_of(item) == "int") {
            int64_t doubled = std::get<int64_t>(item) * 2;
            if (doubled != 200) {
                return 1;
            }
            item = "reassigned";
            if (keet::type_of(item) == "string") {
                std::string s = std::get<std::string>(item);
                if (s == "reassigned") {
                    state = 42;
                }
            }
        }
        if (state != 42) {
            return 2;
        }
        return 0;
    }

    int64_t testNullableInvalidation() {
        std::variant<std::string, std::monostate> token = "session_token_123";
        bool cleared = false;
        if (token != std::monostate{}) {
            std::string active = std::get<std::string>(token);
            if (active != "session_token_123") {
                return 1;
            }
            token = std::monostate{};
            if (token == std::monostate{}) {
                cleared = true;
            }
        }
        if (!cleared) {
            return 2;
        }
        return 0;
    }

    int64_t _keet_main() {
        int64_t r1 = mutateAndVerify();
        if (r1 != 0) {
            return 10 + r1;
        }
        int64_t r2 = testGuardedReassignment();
        if (r2 != 0) {
            return 20 + r2;
        }
        int64_t r3 = testNullableInvalidation();
        if (r3 != 0) {
            return 30 + r3;
        }
        return 0;
    }
} // namespace keet_module_union_narrowing_invalidation

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_union_narrowing_invalidation::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
