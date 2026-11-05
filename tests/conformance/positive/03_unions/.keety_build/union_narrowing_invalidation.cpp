// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "union_narrowing_invalidation.hpp"
#include <iostream>

namespace keet_module_union_narrowing_invalidation {
#line 1 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
    int64_t mutateAndVerify() {
#line 2 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        std::variant<int64_t, std::string, std::monostate> val = 10;
#line 3 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (keet::type_of(val) == "int") {
#line 4 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            int64_t initial = std::get<int64_t>(val);
#line 5 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (initial != 10) {
#line 6 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                return 1;
            }
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            val = "hello";
        }
#line 11 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (keet::type_of(val) == "string") {
#line 12 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            std::string text = std::get<std::string>(val);
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (text != "hello") {
#line 14 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                return 2;
            }
#line 16 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            val = std::monostate{};
        }
#line 19 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (val == std::monostate{}) {
#line 20 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 0;
        }
#line 23 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        return 3;
    }

#line 26 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
    int64_t testGuardedReassignment() {
#line 27 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        std::variant<int64_t, std::string> item = 100;
#line 28 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        int64_t state = 0;
#line 30 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (keet::type_of(item) == "int") {
#line 31 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            int64_t doubled = std::get<int64_t>(item) * 2;
#line 32 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (doubled != 200) {
#line 33 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                return 1;
            }
#line 35 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            item = "reassigned";
#line 36 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (keet::type_of(item) == "string") {
#line 37 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                std::string s = std::get<std::string>(item);
#line 38 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                if (s == "reassigned") {
#line 39 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                    state = 42;
                }
            }
        }
#line 44 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (state != 42) {
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 2;
        }
#line 47 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        return 0;
    }

#line 50 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
    int64_t testNullableInvalidation() {
#line 51 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        std::variant<std::string, std::monostate> token = "session_token_123";
#line 52 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        bool cleared = false;
#line 54 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (token != std::monostate{}) {
#line 55 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            std::string active = std::get<std::string>(token);
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (active != "session_token_123") {
#line 57 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                return 1;
            }
#line 59 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            token = std::monostate{};
#line 60 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            if (token == std::monostate{}) {
#line 61 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
                cleared = true;
            }
        }
#line 65 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (!cleared) {
#line 66 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 2;
        }
#line 68 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        return 0;
    }

#line 71 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
    int64_t _keet_main() {
#line 72 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        int64_t r1 = mutateAndVerify();
#line 73 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (r1 != 0) {
#line 74 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 10 + r1;
        }
#line 77 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        int64_t r2 = testGuardedReassignment();
#line 78 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (r2 != 0) {
#line 79 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 20 + r2;
        }
#line 82 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        int64_t r3 = testNullableInvalidation();
#line 83 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        if (r3 != 0) {
#line 84 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
            return 30 + r3;
        }
#line 87 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/03_unions/union_narrowing_invalidation.keet"
        return 0;
    }
} // namespace keet_module_union_narrowing_invalidation

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_union_narrowing_invalidation::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
