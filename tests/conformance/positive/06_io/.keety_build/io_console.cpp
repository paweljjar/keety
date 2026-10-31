// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "io_console.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_io_console {
    int64_t _keet_main() {
        ::keet::io::print("KeetScript standard I/O test running...\n");
        ::keet::io::print("Primitives: ", 123, " ", 45.67, " ", true, " ", 'K', "\n");
        ::keet::io::println("Line with trailing color", ::keet::io::Color::Green);
        std::vector<int64_t> sample_arr = {10, 20, 30};
        ::keet::io::println("Array output: ", sample_arr);
        ::keet::io::eprint("Diagnostic notice: stderr active\n");
        ::keet::io::eprintln("Diagnostic warning on stderr", ::keet::io::Color::Yellow);
        ::keet::io::flush();
        std::string styled_ok = ::keet::io::green("SUCCESS");
        std::string styled_warn = ::keet::io::yellow("WARNING");
        std::string styled_bold = ::keet::io::bold("BOLD");
        ::keet::io::println("Status:", styled_ok, styled_warn, styled_bold);
        std::string line1 = ::keet::io::readln();
        if (line1 != "hello keetscript") {
            return 1;
        }
        std::string line2 = ::keet::io::input("Enter value: ");
        if (line2 != "sample input") {
            return 2;
        }
        std::variant<std::string, std::monostate> emptyLineOpt = ::keet::io::read_line();
        if (emptyLineOpt == std::monostate{}) {
            return 3;
        }
        std::string emptyStr = std::get<std::string>(emptyLineOpt);
        if (emptyStr != "") {
            return 4;
        }
        std::variant<std::string, std::monostate> eofOpt = ::keet::io::read_line();
        if (eofOpt != std::monostate{}) {
            return 5;
        }
        if (!::keet::io::eof()) {
            return 6;
        }
        ::keet::io::Color defaultColor = ::keet::io::Color::Default;
        ::keet::io::Color redColor = ::keet::io::Color::Red;
        if (defaultColor == redColor) {
            return 7;
        }
        ::keet::io::println("All standard I/O conformance checks passed!");
        return 0;
    }
} // namespace keet_module_io_console

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_io_console::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
