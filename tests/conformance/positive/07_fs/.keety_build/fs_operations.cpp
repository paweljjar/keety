// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#include "fs_operations.hpp"
#include <iostream>
#include <keet/runtime.hpp>

namespace keet_module_fs_operations {
    int64_t _keet_main() {
        ::keet::io::println("Running KeetScript fs conformance operations...");
        std::string cur = ::keet::fs::cwd();
        if (cur == "") {
            return 1;
        }
        if (::keet::fs::filename("dir/test_file.txt") != "test_file.txt") {
            return 2;
        }
        if (::keet::fs::extension("dir/test_file.txt") != ".txt") {
            return 3;
        }
        if (::keet::fs::extension("dir/test_file") != "") {
            return 4;
        }
        if (::keet::fs::parent_dir("dir/sub/file.txt") != "dir/sub") {
            return 5;
        }
        std::string sandbox = ::keet::fs::join("temp_fs_conformance", "inner_dir");
        ::keet::fs::mkdir(sandbox);
        if (!::keet::fs::exists(sandbox) || !::keet::fs::is_dir(sandbox)) {
            return 6;
        }
        std::string target_file = ::keet::fs::join(sandbox, "data.txt");
        ::keet::fs::write_text(target_file, "First Line\nSecond Line\n");
        if (!::keet::fs::exists(target_file) || !::keet::fs::is_file(target_file)) {
            return 7;
        }
        std::string read_str = ::keet::fs::read_text(target_file);
        if (read_str != "First Line\nSecond Line\n") {
            return 8;
        }
        ::keet::fs::append_text(target_file, "Third Line\n");
        std::vector<std::string> lines = ::keet::fs::read_lines(target_file);
        if (lines.size() != 3) {
            return 9;
        }
        if (lines[0] != "First Line" || lines[1] != "Second Line" || lines[2] != "Third Line") {
            return 10;
        }
        int64_t sz = ::keet::fs::file_size(target_file);
        if (sz <= 0) {
            return 11;
        }
        std::string copy_file = ::keet::fs::join(sandbox, "copy.txt");
        ::keet::fs::copy(target_file, copy_file);
        if (!::keet::fs::exists(copy_file)) {
            return 12;
        }
        std::string moved_file = ::keet::fs::join(sandbox, "moved.txt");
        ::keet::fs::rename(copy_file, moved_file);
        if (::keet::fs::exists(copy_file) || !::keet::fs::exists(moved_file)) {
            return 13;
        }
        std::vector<std::string> entries = ::keet::fs::list_dir(sandbox);
        if (entries.size() != 2) {
            return 14;
        }
        std::string missing_path = ::keet::fs::join(sandbox, "missing_file_404.txt");
        std::string fallback_val = ([&]() -> std::decay_t<decltype(::keet::fs::read_text(missing_path))> { try { return ::keet::fs::read_text(missing_path); } catch (...) { return "FALLBACK_VALUE"; } })();
        if (fallback_val != "FALLBACK_VALUE") {
            return 15;
        }
        ::keet::fs::remove(moved_file);
        if (::keet::fs::exists(moved_file)) {
            return 16;
        }
        int64_t count = ::keet::fs::remove_all("temp_fs_conformance");
        if (count <= 0 || ::keet::fs::exists("temp_fs_conformance")) {
            return 17;
        }
        ::keet::io::println("All fs module conformance operations passed successfully!");
        return 0;
    }
} // namespace keet_module_fs_operations

int main(int argc, char* argv[]) {
    try {
        return static_cast<int>(keet_module_fs_operations::_keet_main());
    } catch (const keet::error& err) {
        std::cerr << "Uncaught error: " << err.toString() << "\n";
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught native C++ exception: " << ex.what() << "\n";
        return 1;
    }
}
