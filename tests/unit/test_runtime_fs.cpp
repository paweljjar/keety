#include <catch2/catch_test_macros.hpp>
#include <keet/fs.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace {

struct TempDirGuard {
    std::filesystem::path path;

    TempDirGuard() {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("keety_fs_unit_test_" + std::to_string(now));
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
    }

    ~TempDirGuard() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    [[nodiscard]] std::string file(const std::string& name) const {
        return (path / name).generic_string();
    }
};

} // namespace

TEST_CASE("keet::fs file reading, writing, and appending", "[runtime][fs]") {
    TempDirGuard temp;
    std::string test_file = temp.file("sample.txt");

    SECTION("write_text and read_text roundtrip with UTF-8 text") {
        std::string sample = "Hello KeetScript!\nUTF-8 Symbols: 🚀 🌟 ⚡\nLine 3.";
        REQUIRE(keet::fs::write_text(test_file, sample));
        REQUIRE(keet::fs::exists(test_file));
        REQUIRE(keet::fs::is_file(test_file));
        REQUIRE_FALSE(keet::fs::is_dir(test_file));

        std::string read_back = keet::fs::read_text(test_file);
        REQUIRE(read_back == sample);
        REQUIRE(keet::fs::file_size(test_file) == static_cast<int64_t>(sample.size()));
    }

    SECTION("append_text adds content to existing and new file") {
        std::string part1 = "Part 1\n";
        std::string part2 = "Part 2\n";
        REQUIRE(keet::fs::write_text(test_file, part1));
        REQUIRE(keet::fs::append_text(test_file, part2));

        std::string combined = keet::fs::read_text(test_file);
        REQUIRE(combined == "Part 1\nPart 2\n");

        std::string new_file = temp.file("new_append.txt");
        REQUIRE(keet::fs::append_text(new_file, "Created on append"));
        REQUIRE(keet::fs::read_text(new_file) == "Created on append");
    }

    SECTION("read_lines strips CRLF and LF line endings properly") {
        std::string lines_file = temp.file("lines.txt");
        std::string text_crlf = "First line\r\nSecond line\r\nThird line\r\n";
        REQUIRE(keet::fs::write_text(lines_file, text_crlf));

        auto lines = keet::fs::read_lines(lines_file);
        REQUIRE(lines.size() == 3);
        REQUIRE(lines[0] == "First line");
        REQUIRE(lines[1] == "Second line");
        REQUIRE(lines[2] == "Third line");
    }
}

TEST_CASE("keet::fs directory and file queries and manipulation", "[runtime][fs]") {
    TempDirGuard temp;

    SECTION("mkdir, exists, is_file, is_dir, and remove_all") {
        std::string nested = temp.file("a/b/c/d");
        REQUIRE(keet::fs::mkdir(nested));
        REQUIRE(keet::fs::exists(nested));
        REQUIRE(keet::fs::is_dir(nested));
        REQUIRE_FALSE(keet::fs::is_file(nested));

        std::string dummy_file = temp.file("a/b/c/d/data.txt");
        REQUIRE(keet::fs::write_text(dummy_file, "test data"));
        REQUIRE(keet::fs::exists(dummy_file));
        REQUIRE(keet::fs::is_file(dummy_file));

        int64_t removed_count = keet::fs::remove_all(temp.file("a"));
        REQUIRE(removed_count >= 4);
        REQUIRE_FALSE(keet::fs::exists(temp.file("a")));
    }

    SECTION("copy, rename, remove, and list_dir") {
        std::string dir = temp.file("items_dir");
        REQUIRE(keet::fs::mkdir(dir));

        std::string f1 = temp.file("items_dir/alpha.txt");
        std::string f2 = temp.file("items_dir/beta.txt");
        REQUIRE(keet::fs::write_text(f1, "Alpha content"));
        REQUIRE(keet::fs::write_text(f2, "Beta content"));

        auto list = keet::fs::list_dir(dir);
        REQUIRE(list.size() == 2);
        REQUIRE(list[0] == "alpha.txt");
        REQUIRE(list[1] == "beta.txt");

        std::string f1_copy = temp.file("items_dir/alpha_copy.txt");
        REQUIRE(keet::fs::copy(f1, f1_copy));
        REQUIRE(keet::fs::exists(f1_copy));
        REQUIRE(keet::fs::read_text(f1_copy) == "Alpha content");

        std::string f1_moved = temp.file("items_dir/alpha_moved.txt");
        REQUIRE(keet::fs::rename(f1_copy, f1_moved));
        REQUIRE_FALSE(keet::fs::exists(f1_copy));
        REQUIRE(keet::fs::exists(f1_moved));
        REQUIRE(keet::fs::read_text(f1_moved) == "Alpha content");

        REQUIRE(keet::fs::remove(f1_moved));
        REQUIRE_FALSE(keet::fs::exists(f1_moved));
    }
}

TEST_CASE("keet::fs path normalization and utilities", "[runtime][fs]") {
    SECTION("cwd returns non-empty path") {
        std::string curr = keet::fs::cwd();
        REQUIRE_FALSE(curr.empty());
    }

    SECTION("join, filename, extension, parent_dir") {
        std::string joined = keet::fs::join("project/root", "src/main.keet");
        REQUIRE(joined == "project/root/src/main.keet");

        std::string joined_redundant = keet::fs::join("a/b/../c", "d/./e.txt");
        REQUIRE(joined_redundant == "a/c/d/e.txt");

        REQUIRE(keet::fs::filename("path/to/archive.tar.gz") == "archive.tar.gz");
        REQUIRE(keet::fs::extension("path/to/archive.tar.gz") == ".gz");
        REQUIRE(keet::fs::extension("path/to/file") == "");

        REQUIRE(keet::fs::parent_dir("alpha/beta/gamma.txt") == "alpha/beta");
    }
}

TEST_CASE("keet::fs error handling and zonk interoperability", "[runtime][fs]") {
    TempDirGuard temp;

    SECTION("Reading non-existent file throws keet::error with FileSystemError name") {
        std::string non_existent = temp.file("does_not_exist_404.txt");
        bool caught = false;
        try {
            (void)keet::fs::read_text(non_existent);
        } catch (const keet::error& err) {
            caught = true;
            REQUIRE(err.name == "FileSystemError");
        }
        REQUIRE(caught);
    }

    SECTION("file_size on non-existent file throws keet::error") {
        std::string non_existent = temp.file("no_size.txt");
        bool caught = false;
        try {
            (void)keet::fs::file_size(non_existent);
        } catch (const keet::error& err) {
            caught = true;
            REQUIRE(err.name == "FileSystemError");
        }
        REQUIRE(caught);
    }

    SECTION("keet::Result container wraps success and error") {
        keet::Result<std::string> ok_res("success_payload");
        REQUIRE(ok_res.is_ok());
        REQUIRE_FALSE(ok_res.is_err());
        REQUIRE(ok_res.unwrap() == "success_payload");

        keet::Result<std::string> err_res(keet::error("FileSystemError", "File read error"));
        REQUIRE_FALSE(err_res.is_ok());
        REQUIRE(err_res.is_err());
        REQUIRE(err_res.error().name == "FileSystemError");

        bool unwrap_caught = false;
        try {
            (void)err_res.unwrap();
        } catch (const keet::error& err) {
            unwrap_caught = true;
            REQUIRE(err.name == "FileSystemError");
        }
        REQUIRE(unwrap_caught);
    }
}
