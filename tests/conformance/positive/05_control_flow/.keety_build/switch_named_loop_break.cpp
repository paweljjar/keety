// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "switch_named_loop_break.hpp"
#include <iostream>

namespace keet_module_switch_named_loop_break {
#line 1 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
    int64_t testSwitchNamedWhileBreak() {
#line 2 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t count = 0;
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t step = 0;
#line 4 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        while (step < 10) {
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            step += 1;
#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            switch (step) {
            case 1:
            case 2:
            {
#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                count += 10;
                break;
            }
            case 3:
            {
#line 11 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                count += 30;
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                goto outerLoop_break;
                break;
            }
            default: {
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                count += 100;
                break;
            }
            }
            outerLoop_continue:;
        }
        outerLoop_break:;
#line 19 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (step != 3) {
#line 20 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return 1;
        }
#line 22 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (count != 50) {
#line 23 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return 2;
        }
#line 25 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        return 0;
    }

#line 28 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
    int64_t testSwitchNamedForBreak() {
#line 29 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t accumulated = 0;
#line 30 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        for (int64_t i = 0; i < 20; i += 1) {
#line 31 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            switch (i) {
            case 0:
            case 1:
            case 2:
            {
#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                accumulated += 1;
                break;
            }
            case 3:
            case 4:
            {
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                accumulated += 10;
                break;
            }
            case 5:
            {
#line 39 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                goto forLoop_break;
                break;
            }
            default: {
#line 42 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                accumulated += 1000;
                break;
            }
            }
            forLoop_continue:;
        }
        forLoop_break:;
#line 46 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (accumulated != 23) {
#line 47 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return 10;
        }
#line 49 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        return 0;
    }

#line 52 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
    int64_t testSwitchNamedContinue() {
#line 53 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t sum = 0;
#line 54 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        for (int64_t i = 1; i <= 5; i += 1) {
#line 55 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            switch (i) {
            case 2:
            case 4:
            {
#line 57 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                goto loopC_continue;
                break;
            }
            default: {
#line 60 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
                sum += i;
                break;
            }
            }
            loopC_continue:;
        }
        loopC_break:;
#line 64 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (sum != 9) {
#line 65 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return 20;
        }
#line 67 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        return 0;
    }

#line 70 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
    int64_t _keet_main() {
#line 71 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t r1 = testSwitchNamedWhileBreak();
#line 72 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (r1 != 0) {
#line 73 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return r1;
        }
#line 76 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t r2 = testSwitchNamedForBreak();
#line 77 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (r2 != 0) {
#line 78 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return r2;
        }
#line 81 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        int64_t r3 = testSwitchNamedContinue();
#line 82 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        if (r3 != 0) {
#line 83 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
            return r3;
        }
#line 86 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/05_control_flow/switch_named_loop_break.keet"
        return 0;
    }
} // namespace keet_module_switch_named_loop_break

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_switch_named_loop_break::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
