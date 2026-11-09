// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "zonk_block_yield.hpp"
#include <iostream>

namespace keet_module_zonk_block_yield {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t fallibleCalculation(int64_t input) {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (input < 0) {
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            throw keet::error("Error", "NegativeInput");
        }
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return input * 2;
    }

#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t divideNumbers(int64_t a, int64_t b) {
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return keet::div_safe(a, b);
    }

#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t testCustomErrorRecovery() {
#line 13 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t recovered = ([&]() -> std::decay_t<decltype(fallibleCalculation(-5))> {
            try {
                return fallibleCalculation(-5);
            } catch (const keet::error& err) {
#line 14 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                if (err.message == "NegativeInput") {
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                    return 42;
                }
#line 17 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return -1;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
#line 14 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                if (err.message == "NegativeInput") {
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                    return 42;
                }
#line 17 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return -1;
            }
        })();
#line 19 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (recovered != 42) {
#line 20 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return 1;
        }
#line 23 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t normal = ([&]() -> std::decay_t<decltype(fallibleCalculation(10))> {
            try {
                return fallibleCalculation(10);
            } catch (const keet::error& err) {
#line 24 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return 999;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                return 999;
            }
        })();
#line 26 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (normal != 20) {
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return 2;
        }
#line 29 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return 0;
    }

#line 32 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t testDivZeroRecovery() {
#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t divRes = ([&]() -> std::decay_t<decltype(divideNumbers(100, 0))> {
            try {
                return divideNumbers(100, 0);
            } catch (const keet::error& err) {
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                std::string msg = err.message;
#line 35 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                if (msg == "Integer division by zero") {
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                    return 777;
                }
#line 38 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return -1;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                std::string msg = err.message;
#line 35 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                if (msg == "Integer division by zero") {
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                    return 777;
                }
#line 38 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return -1;
            }
        })();
#line 40 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (divRes != 777) {
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return 10;
        }
#line 43 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return 0;
    }

#line 46 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t testAnonymousRecovery() {
#line 47 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t res = ([&]() -> std::decay_t<decltype(fallibleCalculation(-1))> {
            try {
                return fallibleCalculation(-1);
            } catch (const keet::error& err) {
#line 48 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                int64_t fallbackVal = 10 + 20;
#line 49 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return fallbackVal;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
#line 48 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                int64_t fallbackVal = 10 + 20;
#line 49 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
                return fallbackVal;
            }
        })();
#line 51 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (res != 30) {
#line 52 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return 20;
        }
#line 54 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return 0;
    }

#line 57 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
    int64_t _keet_main() {
#line 58 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t r1 = testCustomErrorRecovery();
#line 59 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (r1 != 0) {
#line 60 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return r1;
        }
#line 63 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t r2 = testDivZeroRecovery();
#line 64 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (r2 != 0) {
#line 65 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return r2;
        }
#line 68 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        int64_t r3 = testAnonymousRecovery();
#line 69 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        if (r3 != 0) {
#line 70 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
            return r3;
        }
#line 73 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/zonk_block_yield.keet"
        return 0;
    }
} // namespace keet_module_zonk_block_yield

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_zonk_block_yield::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
