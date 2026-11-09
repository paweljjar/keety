// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "time_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_time_operations {
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
    int64_t _keet_main() {
#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ::keet::io::println("Running KeetScript time conformance tests...");
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::NS_PER_US != 1000) {
#line 10 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 1;
        }
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::NS_PER_MS != 1000000) {
#line 13 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 2;
        }
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::NS_PER_SEC != 1000000000) {
#line 16 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 3;
        }
#line 18 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::US_PER_MS != 1000) {
#line 19 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 4;
        }
#line 21 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::US_PER_SEC != 1000000) {
#line 22 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 5;
        }
#line 24 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::MS_PER_SEC != 1000) {
#line 25 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 6;
        }
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::SEC_PER_MIN != 60) {
#line 28 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 7;
        }
#line 30 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::SEC_PER_HOUR != 3600) {
#line 31 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 8;
        }
#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::SEC_PER_DAY != 86400) {
#line 34 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 9;
        }
#line 38 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t t0 = ::keet::time::now();
#line 39 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t ms = ::keet::time::nowMs();
#line 40 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t sec = ::keet::time::nowSec();
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t us = ::keet::time::nowUs();
#line 42 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t ns = ::keet::time::nowNs();
#line 44 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (t0 < 1700000000000) {
#line 45 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 10;
        }
#line 47 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (ms < t0) {
#line 48 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 11;
        }
#line 50 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (sec < 1700000000) {
#line 51 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 12;
        }
#line 53 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (us < ms * 1000) {
#line 54 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 13;
        }
#line 56 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (ns < us * 1000) {
#line 57 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 14;
        }
#line 61 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t m0 = ::keet::time::monotonicMs();
#line 62 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t n0 = ::keet::time::monotonicNs();
#line 63 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        double s0 = ::keet::time::monotonicSec();
#line 65 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (m0 < 0 || n0 < 0 || s0 < 0.0) {
#line 66 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 15;
        }
#line 70 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ::keet::time::sleepMs(5);
#line 72 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t el_ms = ::keet::time::elapsedMs(m0);
#line 73 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (el_ms < 4) {
#line 74 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 16;
        }
#line 76 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        double el_sec = ::keet::time::elapsedSec(n0);
#line 77 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (el_sec < 0.004) {
#line 78 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 17;
        }
#line 82 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ::keet::time::sleepUs(500);
#line 83 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ::keet::time::sleepSec(0.002);
#line 86 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        bool zonk_recovered = false;
#line 87 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ([&]() -> std::decay_t<decltype(::keet::time::sleep(-10))> {
            try {
                return ::keet::time::sleep(-10);
            } catch (const keet::error& err) {
#line 88 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
                zonk_recovered = true;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                zonk_recovered = true;
            }
        })();
#line 90 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (!zonk_recovered) {
#line 91 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 18;
        }
#line 95 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        std::string iso0 = ::keet::time::formatIso8601(0);
#line 96 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (iso0 != "1970-01-01T00:00:00Z") {
#line 97 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 19;
        }
#line 99 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::yearUtc(0) != 1970) {
#line 100 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 20;
        }
#line 102 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::monthUtc(0) != 1) {
#line 103 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 21;
        }
#line 105 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::dayUtc(0) != 1) {
#line 106 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 22;
        }
#line 108 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::hourUtc(0) != 0) {
#line 109 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 23;
        }
#line 111 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::minuteUtc(0) != 0) {
#line 112 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 24;
        }
#line 114 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::secondUtc(0) != 0) {
#line 115 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 25;
        }
#line 119 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        int64_t ts = 1700000000000;
#line 120 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        std::string iso_ts = ::keet::time::formatIso8601(ts);
#line 121 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (iso_ts != "2023-11-14T22:13:20Z") {
#line 122 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 26;
        }
#line 124 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::yearUtc(ts) != 2023) {
#line 125 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 27;
        }
#line 127 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::monthUtc(ts) != 11) {
#line 128 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 28;
        }
#line 130 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::dayUtc(ts) != 14) {
#line 131 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 29;
        }
#line 133 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::hourUtc(ts) != 22) {
#line 134 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 30;
        }
#line 136 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::minuteUtc(ts) != 13) {
#line 137 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 31;
        }
#line 139 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (::keet::time::secondUtc(ts) != 20) {
#line 140 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 32;
        }
#line 143 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        std::string date_str = ::keet::time::formatUtc(ts, "%Y-%m-%d");
#line 144 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        if (date_str != "2023-11-14") {
#line 145 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
            return 33;
        }
#line 148 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        ::keet::io::println("All KeetScript time conformance tests passed successfully!");
#line 149 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/10_time/time_operations.keet"
        return 0;
    }
} // namespace keet_module_time_operations

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_time_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
