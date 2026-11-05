// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "fs_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_fs_operations {
#line 4 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
    int64_t _keet_main() {
#line 5 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::io::println("Running KeetScript fs conformance operations...");
#line 8 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string cur = ::keet::fs::cwd();
#line 9 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (cur == "") {
#line 10 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 1;
        }
#line 13 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::fileName("dir/test_file.txt") != "test_file.txt") {
#line 14 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 2;
        }
#line 17 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::extension("dir/test_file.txt") != ".txt") {
#line 18 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 3;
        }
#line 21 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::extension("dir/test_file") != "") {
#line 22 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 4;
        }
#line 25 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::parentDir("dir/sub/file.txt") != "dir/sub") {
#line 26 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 5;
        }
#line 29 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string sandbox = ::keet::fs::join("temp_fs_conformance", "inner_dir");
#line 32 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::mkdir(sandbox);
#line 33 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (!::keet::fs::exists(sandbox) || !::keet::fs::isDir(sandbox)) {
#line 34 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 6;
        }
#line 38 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string target_file = ::keet::fs::join(sandbox, "data.txt");
#line 39 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::writeText(target_file, "First Line\nSecond Line\n");
#line 41 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (!::keet::fs::exists(target_file) || !::keet::fs::isFile(target_file)) {
#line 42 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 7;
        }
#line 45 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string read_str = ::keet::fs::readText(target_file);
#line 46 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (read_str != "First Line\nSecond Line\n") {
#line 47 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 8;
        }
#line 51 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::appendText(target_file, "Third Line\n");
#line 52 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::vector<std::string> lines = ::keet::fs::readLines(target_file);
#line 53 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (lines.size() != 3) {
#line 54 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 9;
        }
#line 56 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (lines[0] != "First Line" || lines[1] != "Second Line" || lines[2] != "Third Line") {
#line 57 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 10;
        }
#line 61 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        int64_t sz = ::keet::fs::fileSize(target_file);
#line 62 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (sz <= 0) {
#line 63 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 11;
        }
#line 67 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string copy_file = ::keet::fs::join(sandbox, "copy.txt");
#line 68 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::copy(target_file, copy_file);
#line 69 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (!::keet::fs::exists(copy_file)) {
#line 70 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 12;
        }
#line 73 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string moved_file = ::keet::fs::join(sandbox, "moved.txt");
#line 74 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::rename(copy_file, moved_file);
#line 75 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::exists(copy_file) || !::keet::fs::exists(moved_file)) {
#line 76 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 13;
        }
#line 80 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::vector<std::string> entries = ::keet::fs::listDir(sandbox);
#line 81 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (entries.size() != 2) {
#line 82 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 14;
        }
#line 86 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string missing_path = ::keet::fs::join(sandbox, "missing_file_404.txt");
#line 87 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        std::string fallback_val = ([&]() -> std::decay_t<decltype(::keet::fs::readText(missing_path))> { try { return ::keet::fs::readText(missing_path); } catch (...) { return "FALLBACK_VALUE"; } })();
#line 88 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (fallback_val != "FALLBACK_VALUE") {
#line 89 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 15;
        }
#line 93 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::fs::remove(moved_file);
#line 94 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (::keet::fs::exists(moved_file)) {
#line 95 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 16;
        }
#line 98 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        int64_t count = ::keet::fs::removeAll("temp_fs_conformance");
#line 99 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        if (count <= 0 || ::keet::fs::exists("temp_fs_conformance")) {
#line 100 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
            return 17;
        }
#line 103 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        ::keet::io::println("All fs module conformance operations passed successfully!");
#line 104 "/Users/pawel/CLionProjects/keety/tests/conformance/positive/07_fs/fs_operations.keet"
        return 0;
    }
} // namespace keet_module_fs_operations

int main(int argc, char* argv[]) {
    try {
        ::keet::sys::initArgs(argc, argv);
        return static_cast<int>(keet_module_fs_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
