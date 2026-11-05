// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "sys_environment.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_sys_environment {
#line 4 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
    int64_t _keet_main() {
#line 5 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        ::keet::io::println("Running KeetScript sys conformance tests...");
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::vector<std::string> args = ::keet::sys::args();
#line 9 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        int64_t pid = ::keet::sys::pid();
#line 10 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (pid <= 0) {
#line 11 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 1;
        }
#line 15 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string plat = ::keet::sys::platform();
#line 16 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (plat == "") {
#line 17 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 2;
        }
#line 20 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string arch = ::keet::sys::arch();
#line 21 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (arch == "") {
#line 22 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 3;
        }
#line 25 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        int64_t cpus = ::keet::sys::cpuCount();
#line 26 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (cpus < 1) {
#line 27 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 4;
        }
#line 31 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string cur = ::keet::sys::cwd();
#line 32 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (cur == "") {
#line 33 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 5;
        }
#line 36 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string tmp = ::keet::sys::tempDir();
#line 37 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (tmp == "") {
#line 38 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 6;
        }
#line 42 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string test_key = "KEET_CONFORMANCE_SYS_TEST";
#line 43 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string test_val = "keet_sys_value_789";
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        ::keet::sys::unsetEnv(test_key);
#line 46 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (::keet::sys::hasEnv(test_key)) {
#line 47 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 7;
        }
#line 50 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (::keet::sys::getEnvOr(test_key, "default_val") != "default_val") {
#line 51 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 8;
        }
#line 54 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        ::keet::sys::setEnv(test_key, test_val);
#line 55 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (!::keet::sys::hasEnv(test_key)) {
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 9;
        }
#line 59 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::variant<std::string, std::monostate> env_val = ::keet::sys::getEnv(test_key);
#line 60 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (env_val == std::monostate{}) {
#line 61 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 10;
        }
#line 64 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (::keet::sys::getEnvOr(test_key, "fallback") != test_val) {
#line 65 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 11;
        }
#line 68 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        ::keet::sys::unsetEnv(test_key);
#line 69 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (::keet::sys::hasEnv(test_key)) {
#line 70 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 12;
        }
#line 74 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string echo_out = ([&]() -> std::decay_t<decltype(::keet::sys::execOutput("echo keet_sys_conform"))> { try { return ::keet::sys::execOutput("echo keet_sys_conform"); } catch (...) { return ""; } })();
#line 75 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (echo_out == "") {
#line 76 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 13;
        }
#line 79 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        std::string recovered = ([&]() -> std::decay_t<decltype(::keet::sys::execOutput("false || exit 1"))> { try { return ::keet::sys::execOutput("false || exit 1"); } catch (...) { return "fallback_recovery"; } })();
#line 80 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        if (recovered != "fallback_recovery") {
#line 81 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
            return 14;
        }
#line 84 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        ::keet::io::println("KeetScript sys conformance tests passed successfully!");
#line 85 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/08_sys/sys_environment.keet"
        return 0;
    }
} // namespace keet_module_sys_environment

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_sys_environment::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
