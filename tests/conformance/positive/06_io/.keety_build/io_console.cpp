// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "io_console.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_io_console {
#line 3 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
    int64_t _keet_main() {
#line 4 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::print("KeetScript standard I/O test running...\n");
#line 5 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::print("Primitives: ", 123, " ", 45.67, " ", true, " ", 'K', "\n");
#line 6 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::println("Line with trailing color", ::keet::io::Color::Green);
#line 7 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::vector<int64_t> sample_arr = {10, 20, 30};
#line 8 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::variant<int64_t, std::string> sample_union = "Array element: ";
#line 9 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::println(sample_union, sample_arr[0]);
#line 11 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::eprint("Diagnostic notice: stderr active\n");
#line 12 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::eprintln("Diagnostic warning on stderr", ::keet::io::Color::Yellow);
#line 13 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::flush();
#line 15 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string styled_ok = ::keet::io::green("SUCCESS");
#line 16 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string styled_warn = ::keet::io::yellow("WARNING");
#line 17 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string styled_bold = ::keet::io::bold("BOLD");
#line 18 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::println("Status:", styled_ok, styled_warn, styled_bold);
#line 20 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string line1 = ::keet::io::readln();
#line 21 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (line1 != "hello keetscript") {
#line 22 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 1;
        }
#line 25 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string line2 = ::keet::io::input("Enter value: ");
#line 26 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (line2 != "sample input") {
#line 27 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 2;
        }
#line 31 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::variant<std::string, std::monostate> emptyLineOpt = ::keet::io::readLine();
#line 32 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (emptyLineOpt == std::monostate{}) {
#line 33 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 3;
        }
#line 35 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::string emptyStr = std::get<std::string>(emptyLineOpt);
#line 36 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (emptyStr != "") {
#line 37 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 4;
        }
#line 41 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        std::variant<std::string, std::monostate> eofOpt = ::keet::io::readLine();
#line 42 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (eofOpt != std::monostate{}) {
#line 43 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 5;
        }
#line 46 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (!::keet::io::eof()) {
#line 47 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 6;
        }
#line 50 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::Color defaultColor = ::keet::io::Color::Default;
#line 51 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::Color redColor = ::keet::io::Color::Red;
#line 52 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        if (defaultColor == redColor) {
#line 53 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
            return 7;
        }
#line 56 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        ::keet::io::println("All standard I/O conformance checks passed!");
#line 57 "/Users/pawel/Documents/GitHub/keety/tests/conformance/positive/06_io/io_console.keet"
        return 0;
    }
} // namespace keet_module_io_console

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_io_console::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
