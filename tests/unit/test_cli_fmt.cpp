#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::driver;

namespace {

struct TempDirectory {
    std::filesystem::path path;

    TempDirectory() {
        path = std::filesystem::temp_directory_path() / ("keety_cli_fmt_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    [[nodiscard]] std::filesystem::path createFile(const std::string& relativePath, const std::string& content) {
        auto fullPath = (path / std::filesystem::path(relativePath)).make_preferred();
        std::filesystem::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath, std::ios::binary);
        ofs << content;
        ofs.close();
        return fullPath;
    }

    [[nodiscard]] std::string readFile(const std::filesystem::path& target) {
        std::ifstream ifs(target, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }
};

} // namespace

TEST_CASE("CLI argument parser handles keety fmt", "[driver][cli][fmt]") {
    SECTION("Parse basic fmt command with file target") {
        std::vector<std::string> args = {"fmt", "src/main.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        REQUIRE(opts.fmt_paths.size() == 1);
        CHECK(opts.fmt_paths[0] == "src/main.keet");
        CHECK(opts.fmt_write == true);
        CHECK(opts.fmt_check == false);
        CHECK(opts.fmt_diff == false);
        CHECK(opts.fmt_stdin == false);
        CHECK(opts.fmt_recursive == true);
    }

    SECTION("Parse fmt --check flag") {
        std::vector<std::string> args = {"fmt", "--check", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        CHECK(opts.fmt_check == true);
        CHECK(opts.fmt_write == false);
        CHECK(opts.fmt_diff == false);
        REQUIRE(opts.fmt_paths.size() == 1);
        CHECK(opts.fmt_paths[0] == "src/app.keet");
    }

    SECTION("Parse fmt --diff flag") {
        std::vector<std::string> args = {"fmt", "--diff", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        CHECK(opts.fmt_diff == true);
        CHECK(opts.fmt_write == false);
        CHECK(opts.fmt_check == false);
    }

    SECTION("Parse fmt --diff with explicit -w / --write") {
        std::vector<std::string> args = {"fmt", "--diff", "-w", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        CHECK(opts.fmt_diff == true);
        CHECK(opts.fmt_write == true);
    }

    SECTION("Parse fmt stdin indicator '-'") {
        std::vector<std::string> args = {"fmt", "-"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        CHECK(opts.fmt_stdin == true);
        CHECK(opts.fmt_paths.empty());
    }

    SECTION("Parse multiple fmt paths") {
        std::vector<std::string> args = {"fmt", "src/", "tests/", "main.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Fmt);
        REQUIRE(opts.fmt_paths.size() == 3);
        CHECK(opts.fmt_paths[0] == "src/");
        CHECK(opts.fmt_paths[1] == "tests/");
        CHECK(opts.fmt_paths[2] == "main.keet");
    }

    SECTION("String conversion for Fmt command") {
        CHECK(to_string(CliCommand::Fmt) == "fmt");
    }
}

TEST_CASE("CLI driver execute_fmt stdin mode", "[driver][cli][fmt]") {
    SECTION("Standard pipe format to stdout") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_stdin = true;

        std::istringstream in("fn   main  (  )  : void   {\nlet   x : int=  10 ; \n}\n");
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(out.str() == "fn main(): void {\n    let x: int = 10;\n}\n");
        CHECK(err.str().empty());
    }

    SECTION("Stdin mode with --check on unformatted code returns 1") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_stdin = true;
        opts.fmt_check = true;

        std::istringstream in("let  a :int =1 ; \n");
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 1);
        CHECK(err.str() == "<stdin>\n");
        CHECK(out.str().empty());
    }

    SECTION("Stdin mode with --check on formatted code returns 0") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_stdin = true;
        opts.fmt_check = true;

        std::istringstream in("let a: int = 1;\n");
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(err.str().empty());
        CHECK(out.str().empty());
    }

    SECTION("Stdin mode with --diff on unformatted code prints unified diff") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_stdin = true;
        opts.fmt_diff = true;

        std::istringstream in("let  a :int =1 ;\n");
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(out.str().find("--- a/<stdin>") != std::string::npos);
        CHECK(out.str().find("+++ b/<stdin>") != std::string::npos);
        CHECK(out.str().find("-let  a :int =1 ;") != std::string::npos);
        CHECK(out.str().find("+let a: int = 1;") != std::string::npos);
    }

    SECTION("Stdin mode with syntax error returns 1 and emits diagnostics") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_stdin = true;

        std::istringstream in("fn { invalid syntax }}}");
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 1);
        CHECK(!err.str().empty());
    }
}

TEST_CASE("CLI driver execute_fmt file operations", "[driver][cli][fmt]") {
    TempDirectory tempDir;

    SECTION("In-place write formats target file") {
        auto filePath = tempDir.createFile("messy.keet", "fn  calc ( a:int ,b:int ) :int { \n return a+b;\n}\n");

        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(filePath);
        opts.fmt_write = true;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(err.str().empty());

        std::string updatedContent = tempDir.readFile(filePath);
        CHECK(updatedContent == "fn calc(a: int, b: int): int {\n    return a + b;\n}\n");

        // Running format again should be idempotent
        int secondRes = CliDriver::run(opts, out, err);
        CHECK(secondRes == 0);
        CHECK(tempDir.readFile(filePath) == updatedContent);
    }

    SECTION("File mode with --check returns 1 on unformatted file without modifying it") {
        std::string unformatted = "let   val : string  = \"hello\" ;\n";
        auto filePath = tempDir.createFile("check_me.keet", unformatted);

        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(filePath);
        opts.fmt_check = true;
        opts.fmt_write = false;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find(filePath.string()) != std::string::npos);
        // File must NOT be modified in check mode
        CHECK(tempDir.readFile(filePath) == unformatted);
    }

    SECTION("File mode with --check returns 0 on formatted file") {
        auto filePath = tempDir.createFile("clean.keet", "let val: string = \"hello\";\n");

        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(filePath);
        opts.fmt_check = true;
        opts.fmt_write = false;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(err.str().empty());
    }

    SECTION("File mode with --diff outputs unified diff without modifying file") {
        std::string unformatted = "let   num : int = 99 ;\n";
        auto filePath = tempDir.createFile("diff_me.keet", unformatted);

        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(filePath);
        opts.fmt_diff = true;
        opts.fmt_write = false;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(out.str().find("--- a/" + filePath.string()) != std::string::npos);
        CHECK(out.str().find("+++ b/" + filePath.string()) != std::string::npos);
        CHECK(out.str().find("-let   num : int = 99 ;") != std::string::npos);
        CHECK(out.str().find("+let num: int = 99;") != std::string::npos);
        // File content unchanged
        CHECK(tempDir.readFile(filePath) == unformatted);
    }
}

TEST_CASE("CLI driver execute_fmt directory recursion and file discovery", "[driver][cli][fmt]") {
    TempDirectory tempDir;

    auto cleanFile = tempDir.createFile("clean.keet", "let x: int = 1;\n");
    auto messyFile = tempDir.createFile("nested/dir/messy.keet", "let   y   :int =2 ; \n");
    auto nonKeetFile = tempDir.createFile("nested/other.txt", "unrelated text file");
    auto hiddenKeetFile = tempDir.createFile(".hidden/secret.keet", "let   z : int = 3;\n");

    SECTION("discover_keet_files discovers only valid .keet files in non-hidden directories") {
        auto discovered = CliDriver::discover_keet_files(tempDir.path);

        CHECK(discovered.size() == 2);
        bool foundClean = false;
        bool foundMessy = false;
        for (const auto& p : discovered) {
            if (p == cleanFile) foundClean = true;
            if (p == messyFile) foundMessy = true;
        }
        CHECK(foundClean);
        CHECK(foundMessy);
    }

    SECTION("Recursive --check detects unformatted files in directory") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(tempDir.path);
        opts.fmt_check = true;
        opts.fmt_write = false;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK((err.str().find(messyFile.string()) != std::string::npos ||
               err.str().find(messyFile.generic_string()) != std::string::npos));
        CHECK((err.str().find(cleanFile.string()) == std::string::npos &&
               err.str().find(cleanFile.generic_string()) == std::string::npos));
    }

    SECTION("Recursive formatting in-place cleans all files in directory") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(tempDir.path);
        opts.fmt_write = true;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);

        CHECK(tempDir.readFile(messyFile) == "let y: int = 2;\n");
        CHECK(tempDir.readFile(cleanFile) == "let x: int = 1;\n");

        // Now --check on the directory should pass with 0
        opts.fmt_check = true;
        opts.fmt_write = false;
        out.str(""); err.str("");
        int checkRes = CliDriver::run(opts, out, err);
        CHECK(checkRes == 0);
        CHECK(err.str().empty());
    }
}

TEST_CASE("CLI driver execute_fmt error handling", "[driver][cli][fmt]") {
    SECTION("Non-existent path returns 1 and error message") {
        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back("/path/does/not/exist/anywhere/foo.keet");

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("path not found") != std::string::npos);
    }

    SECTION("File with syntax errors returns 1 and compiler diagnostics") {
        TempDirectory tempDir;
        auto badFile = tempDir.createFile("broken.keet", "class { syntax error !!!");

        CliOptions opts;
        opts.command = CliCommand::Fmt;
        opts.fmt_paths.push_back(badFile);

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(!err.str().empty());
    }
}
