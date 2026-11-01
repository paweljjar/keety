// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "sys_environment.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_sys_environment {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript sys conformance tests...");
        std::vector<std::string> args = ::keet::sys::args();
        int64_t pid = ::keet::sys::pid();
        if (pid <= 0) {
            return 1;
        }
        std::string plat = ::keet::sys::platform();
        if (plat == "") {
            return 2;
        }
        std::string arch = ::keet::sys::arch();
        if (arch == "") {
            return 3;
        }
        int64_t cpus = ::keet::sys::cpuCount();
        if (cpus < 1) {
            return 4;
        }
        std::string cur = ::keet::sys::cwd();
        if (cur == "") {
            return 5;
        }
        std::string tmp = ::keet::sys::tempDir();
        if (tmp == "") {
            return 6;
        }
        std::string test_key = "KEET_CONFORMANCE_SYS_TEST";
        std::string test_val = "keet_sys_value_789";
        ::keet::sys::unsetEnv(test_key);
        if (::keet::sys::hasEnv(test_key)) {
            return 7;
        }
        if (::keet::sys::getEnvOr(test_key, "default_val") != "default_val") {
            return 8;
        }
        ::keet::sys::setEnv(test_key, test_val);
        if (!::keet::sys::hasEnv(test_key)) {
            return 9;
        }
        std::variant<std::string, std::monostate> env_val = ::keet::sys::getEnv(test_key);
        if (env_val == std::monostate{}) {
            return 10;
        }
        if (::keet::sys::getEnvOr(test_key, "fallback") != test_val) {
            return 11;
        }
        ::keet::sys::unsetEnv(test_key);
        if (::keet::sys::hasEnv(test_key)) {
            return 12;
        }
        std::string echo_out = ([&]() -> std::decay_t<decltype(::keet::sys::execOutput("echo keet_sys_conform"))> { try { return ::keet::sys::execOutput("echo keet_sys_conform"); } catch (...) { return ""; } })();
        if (echo_out == "") {
            return 13;
        }
        std::string recovered = ([&]() -> std::decay_t<decltype(::keet::sys::execOutput("false || exit 1"))> { try { return ::keet::sys::execOutput("false || exit 1"); } catch (...) { return "fallback_recovery"; } })();
        if (recovered != "fallback_recovery") {
            return 14;
        }
        ::keet::io::println("KeetScript sys conformance tests passed successfully!");
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
