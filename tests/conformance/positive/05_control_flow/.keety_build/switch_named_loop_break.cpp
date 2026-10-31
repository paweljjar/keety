// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "switch_named_loop_break.hpp"
#include <iostream>

namespace keet_module_switch_named_loop_break {
    int64_t testSwitchNamedWhileBreak() {
        int64_t count = 0;
        int64_t step = 0;
        while (step < 10) {
            step += 1;
            switch (step) {
            case 1:
            case 2:
            {
                count += 10;
                break;
            }
            case 3:
            {
                count += 30;
                goto outerLoop_break;
                break;
            }
            default: {
                count += 100;
                break;
            }
            }
            outerLoop_continue:;
        }
        outerLoop_break:;
        if (step != 3) {
            return 1;
        }
        if (count != 50) {
            return 2;
        }
        return 0;
    }

    int64_t testSwitchNamedForBreak() {
        int64_t accumulated = 0;
        for (int64_t i = 0; i < 20; i += 1) {
            switch (i) {
            case 0:
            case 1:
            case 2:
            {
                accumulated += 1;
                break;
            }
            case 3:
            case 4:
            {
                accumulated += 10;
                break;
            }
            case 5:
            {
                goto forLoop_break;
                break;
            }
            default: {
                accumulated += 1000;
                break;
            }
            }
            forLoop_continue:;
        }
        forLoop_break:;
        if (accumulated != 23) {
            return 10;
        }
        return 0;
    }

    int64_t testSwitchNamedContinue() {
        int64_t sum = 0;
        for (int64_t i = 1; i <= 5; i += 1) {
            switch (i) {
            case 2:
            case 4:
            {
                goto loopC_continue;
                break;
            }
            default: {
                sum += i;
                break;
            }
            }
            loopC_continue:;
        }
        loopC_break:;
        if (sum != 9) {
            return 20;
        }
        return 0;
    }

    int64_t _keet_main() {
        int64_t r1 = testSwitchNamedWhileBreak();
        if (r1 != 0) {
            return r1;
        }
        int64_t r2 = testSwitchNamedForBreak();
        if (r2 != 0) {
            return r2;
        }
        int64_t r3 = testSwitchNamedContinue();
        if (r3 != 0) {
            return r3;
        }
        return 0;
    }
} // namespace keet_module_switch_named_loop_break

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_switch_named_loop_break::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
