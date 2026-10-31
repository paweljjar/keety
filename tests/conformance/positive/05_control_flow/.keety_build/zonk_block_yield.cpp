// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "zonk_block_yield.hpp"
#include <iostream>

namespace keet_module_zonk_block_yield {
    int64_t fallibleCalculation(int64_t input) {
        if (input < 0) {
            throw keet::error("Error", "NegativeInput");
        }
        return input * 2;
    }

    int64_t divideNumbers(int64_t a, int64_t b) {
        return a / b;
    }

    int64_t testCustomErrorRecovery() {
        int64_t recovered = ([&]() -> std::decay_t<decltype(fallibleCalculation(-5))> {
            try {
                return fallibleCalculation(-5);
            } catch (const keet::error& err) {
                if (err.message == "NegativeInput") {
                    return 42;
                }
                return -1;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                if (err.message == "NegativeInput") {
                    return 42;
                }
                return -1;
            }
        })();
        if (recovered != 42) {
            return 1;
        }
        int64_t normal = ([&]() -> std::decay_t<decltype(fallibleCalculation(10))> {
            try {
                return fallibleCalculation(10);
            } catch (const keet::error& err) {
                return 999;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                return 999;
            }
        })();
        if (normal != 20) {
            return 2;
        }
        return 0;
    }

    int64_t testDivZeroRecovery() {
        int64_t divRes = ([&]() -> std::decay_t<decltype(divideNumbers(100, 0))> {
            try {
                return divideNumbers(100, 0);
            } catch (const keet::error& err) {
                std::string msg = err.message;
                if (msg == "Integer division by zero") {
                    return 777;
                }
                return -1;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                std::string msg = err.message;
                if (msg == "Integer division by zero") {
                    return 777;
                }
                return -1;
            }
        })();
        if (divRes != 777) {
            return 10;
        }
        return 0;
    }

    int64_t testAnonymousRecovery() {
        int64_t res = ([&]() -> std::decay_t<decltype(fallibleCalculation(-1))> {
            try {
                return fallibleCalculation(-1);
            } catch (const keet::error& err) {
                int64_t fallbackVal = 10 + 20;
                return fallbackVal;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                int64_t fallbackVal = 10 + 20;
                return fallbackVal;
            }
        })();
        if (res != 30) {
            return 20;
        }
        return 0;
    }

    int64_t _keet_main() {
        int64_t r1 = testCustomErrorRecovery();
        if (r1 != 0) {
            return r1;
        }
        int64_t r2 = testDivZeroRecovery();
        if (r2 != 0) {
            return r2;
        }
        int64_t r3 = testAnonymousRecovery();
        if (r3 != 0) {
            return r3;
        }
        return 0;
    }
} // namespace keet_module_zonk_block_yield

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_zonk_block_yield::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
