// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "time_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_time_operations {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript time conformance tests...");
        if (::keet::time::NS_PER_US != 1000) {
            return 1;
        }
        if (::keet::time::NS_PER_MS != 1000000) {
            return 2;
        }
        if (::keet::time::NS_PER_SEC != 1000000000) {
            return 3;
        }
        if (::keet::time::US_PER_MS != 1000) {
            return 4;
        }
        if (::keet::time::US_PER_SEC != 1000000) {
            return 5;
        }
        if (::keet::time::MS_PER_SEC != 1000) {
            return 6;
        }
        if (::keet::time::SEC_PER_MIN != 60) {
            return 7;
        }
        if (::keet::time::SEC_PER_HOUR != 3600) {
            return 8;
        }
        if (::keet::time::SEC_PER_DAY != 86400) {
            return 9;
        }
        int64_t t0 = ::keet::time::now();
        int64_t ms = ::keet::time::nowMs();
        int64_t sec = ::keet::time::nowSec();
        int64_t us = ::keet::time::nowUs();
        int64_t ns = ::keet::time::nowNs();
        if (t0 < 1700000000000) {
            return 10;
        }
        if (ms < t0) {
            return 11;
        }
        if (sec < 1700000000) {
            return 12;
        }
        if (us < ms * 1000) {
            return 13;
        }
        if (ns < us * 1000) {
            return 14;
        }
        int64_t m0 = ::keet::time::monotonicMs();
        int64_t n0 = ::keet::time::monotonicNs();
        double s0 = ::keet::time::monotonicSec();
        if (m0 < 0 || n0 < 0 || s0 < 0.0) {
            return 15;
        }
        ::keet::time::sleepMs(5);
        int64_t el_ms = ::keet::time::elapsedMs(m0);
        if (el_ms < 4) {
            return 16;
        }
        double el_sec = ::keet::time::elapsedSec(n0);
        if (el_sec < 0.004) {
            return 17;
        }
        ::keet::time::sleepUs(500);
        ::keet::time::sleepSec(0.002);
        bool zonk_recovered = false;
        ([&]() -> std::decay_t<decltype(::keet::time::sleep(-10))> {
            try {
                return ::keet::time::sleep(-10);
            } catch (const keet::error& err) {
                zonk_recovered = true;
            } catch (const std::exception& e) {
                const keet::error err("Error", e.what());
                zonk_recovered = true;
            }
        })();
        if (!zonk_recovered) {
            return 18;
        }
        std::string iso0 = ::keet::time::formatIso8601(0);
        if (iso0 != "1970-01-01T00:00:00Z") {
            return 19;
        }
        if (::keet::time::yearUtc(0) != 1970) {
            return 20;
        }
        if (::keet::time::monthUtc(0) != 1) {
            return 21;
        }
        if (::keet::time::dayUtc(0) != 1) {
            return 22;
        }
        if (::keet::time::hourUtc(0) != 0) {
            return 23;
        }
        if (::keet::time::minuteUtc(0) != 0) {
            return 24;
        }
        if (::keet::time::secondUtc(0) != 0) {
            return 25;
        }
        int64_t ts = 1700000000000;
        std::string iso_ts = ::keet::time::formatIso8601(ts);
        if (iso_ts != "2023-11-14T22:13:20Z") {
            return 26;
        }
        if (::keet::time::yearUtc(ts) != 2023) {
            return 27;
        }
        if (::keet::time::monthUtc(ts) != 11) {
            return 28;
        }
        if (::keet::time::dayUtc(ts) != 14) {
            return 29;
        }
        if (::keet::time::hourUtc(ts) != 22) {
            return 30;
        }
        if (::keet::time::minuteUtc(ts) != 13) {
            return 31;
        }
        if (::keet::time::secondUtc(ts) != 20) {
            return 32;
        }
        std::string date_str = ::keet::time::formatUtc(ts, "%Y-%m-%d");
        if (date_str != "2023-11-14") {
            return 33;
        }
        ::keet::io::println("All KeetScript time conformance tests passed successfully!");
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
