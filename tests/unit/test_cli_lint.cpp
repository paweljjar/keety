#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::driver;

namespace {

struct TempDirectory {
    std::filesystem::path path;

    TempDirectory() {
        path = std::filesystem::temp_directory_path() / ("keety_cli_lint_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
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

TEST_CASE("CLI argument parser handles keety lint", "[driver][cli][lint]") {
    SECTION("Parse basic lint command with target path") {
        std::vector<std::string> args = {"lint", "src/main.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        REQUIRE(opts.lint_paths.size() == 1);
        CHECK(opts.lint_paths[0] == "src/main.keet");
        CHECK(opts.lint_check == false);
        CHECK(opts.lint_fix == false);
        CHECK(opts.lint_strict == false);
        CHECK(opts.lint_stdin == false);
    }

    SECTION("Parse lint --check flag") {
        std::vector<std::string> args = {"lint", "--check", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        CHECK(opts.lint_check == true);
        CHECK(opts.lint_fix == false);
        REQUIRE(opts.lint_paths.size() == 1);
        CHECK(opts.lint_paths[0] == "src/app.keet");
    }

    SECTION("Parse lint --fix flag") {
        std::vector<std::string> args = {"lint", "--fix", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        CHECK(opts.lint_fix == true);
        CHECK(opts.lint_check == false);
    }

    SECTION("Parse lint --strict flag") {
        std::vector<std::string> args = {"lint", "--strict", "src/app.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        CHECK(opts.lint_strict == true);
    }

    SECTION("Parse lint stdin flag '-'") {
        std::vector<std::string> args = {"lint", "-"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        CHECK(opts.lint_stdin == true);
        CHECK(opts.lint_paths.empty());
    }

    SECTION("Parse lint with --json diagnostics flag") {
        std::vector<std::string> args = {"lint", "--json", "src/main.keet"};
        auto opts = CliDriver::parse_args(args);

        CHECK(opts.command == CliCommand::Lint);
        CHECK(opts.diagnostic_format == DiagnosticFormat::Json);
    }

    SECTION("String conversion for Lint command") {
        CHECK(to_string(CliCommand::Lint) == "lint");
    }
}

TEST_CASE("CLI driver execute_lint stdin mode", "[driver][cli][lint]") {
    SECTION("Clean source code produces exit 0 and empty stderr") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;

        std::string src =
            "import io;\n"
            "fn main(): void {\n"
            "    io.println(\"clean\");\n"
            "}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(err.str().empty());
    }

    SECTION("Source with warnings produces exit 0 by default and emits warnings to stderr") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;

        std::string src =
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"hello\");\n"
            "}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(err.str().find("L001") != std::string::npos);
        CHECK(err.str().find("math") != std::string::npos);
    }

    SECTION("Source with warnings and --check produces exit code 1") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;
        opts.lint_check = true;

        std::string src =
            "import math;\n"
            "fn main(): void {}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 1);
        CHECK(err.str().find("L001") != std::string::npos);
    }

    SECTION("Source with warnings and --strict elevates warnings to errors and exits 1") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;
        opts.lint_strict = true;

        std::string src =
            "import math;\n"
            "fn main(): void {}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 1);
        CHECK(err.str().find("error[L001]") != std::string::npos);
    }

    SECTION("Stdin mode with --fix applies edits and writes to stdout") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;
        opts.lint_fix = true;

        std::string src =
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"hello\");\n"
            "}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        // import math; should be removed from stdout
        CHECK(out.str().find("import math;") == std::string::npos);
        CHECK(out.str().find("import io;") != std::string::npos);
    }

    SECTION("Stdin mode with syntax error exits 1 and emits error") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;

        std::string src = "fn broken( { \n";
        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 1);
        CHECK(err.str().find("error") != std::string::npos);
    }

    SECTION("Stdin mode with --json emits structured JSON array") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_stdin = true;
        opts.diagnostic_format = DiagnosticFormat::Json;

        std::string src =
            "import math;\n"
            "fn main(): void {}\n";

        std::istringstream in(src);
        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err, in);
        CHECK(res == 0);
        CHECK(out.str().front() == '[');
        CHECK(out.str().find("\"code\": \"L001\"") != std::string::npos);
    }
}

TEST_CASE("CLI driver execute_lint file and directory mode", "[driver][cli][lint]") {
    TempDirectory tempDir;

    SECTION("Clean file lint returns exit 0") {
        auto file = tempDir.createFile("clean.keet",
            "import io;\n"
            "fn main(): void {\n"
            "    io.println(\"ok\");\n"
            "}\n"
        );

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(file);

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(err.str().empty());
    }

    SECTION("File with warning returns 0 by default and 1 with --check") {
        auto file = tempDir.createFile("warn.keet",
            "import math;\n"
            "fn main(): void {}\n"
        );

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(file);

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(err.str().find("L001") != std::string::npos);

        opts.lint_check = true;
        std::ostringstream checkErr;
        int checkRes = CliDriver::run(opts, out, checkErr);
        CHECK(checkRes == 1);
        CHECK(checkErr.str().find("L001") != std::string::npos);
    }

    SECTION("File with --strict treats warning as error and returns 1") {
        auto file = tempDir.createFile("strict.keet",
            "import math;\n"
            "fn main(): void {}\n"
        );

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(file);
        opts.lint_strict = true;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("error[L001]") != std::string::npos);
    }

    SECTION("Non-existent path returns 1 with error message") {
        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back("non_existent_dir_12345/missing.keet");

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 1);
        CHECK(err.str().find("path not found") != std::string::npos);
    }

    SECTION("File mode with --fix modifies file in-place and clears violations") {
        auto file = tempDir.createFile("fix_me.keet",
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"fixed\");\n"
            "}\n"
        );

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(file);
        opts.lint_fix = true;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);

        std::string updated = tempDir.readFile(file);
        CHECK(updated.find("import math;") == std::string::npos);
        CHECK(updated.find("import io;") != std::string::npos);

        // Subsequent check run should be completely clean
        opts.lint_fix = false;
        opts.lint_check = true;
        std::ostringstream cleanOut, cleanErr;
        int cleanRes = CliDriver::run(opts, cleanOut, cleanErr);
        CHECK(cleanRes == 0);
        CHECK(cleanErr.str().empty());
    }

    SECTION("Directory recursion discovers .keet files and ignores .hidden") {
        auto sub1 = tempDir.createFile("sub/file1.keet", "import math;\nfn f(): void {}\n");
        auto sub2 = tempDir.createFile("sub/nested/file2.keet", "import io;\nfn g(): void { io.println(\"g\"); }\n");
        auto hidden = tempDir.createFile(".hidden/secret.keet", "import math;\n");
        auto txt = tempDir.createFile("sub/readme.txt", "text");

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(tempDir.path);

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        // Only sub1 should have emitted an L001 warning; hidden must not be scanned
        CHECK(err.str().find("file1.keet") != std::string::npos);
        CHECK(err.str().find(".hidden") == std::string::npos);
    }

    SECTION("File mode with .keetlint.json auto-discovery disables rule") {
        (void)tempDir.createFile(".keetlint.json", "{\n  \"rules\": {\n    \"unused-import\": \"off\"\n  }\n}\n");
        auto file = tempDir.createFile("app.keet",
            "import math;\n"
            "fn main(): void {\n"
            "    return;\n"
            "}\n"
        );

        CliOptions opts;
        opts.command = CliCommand::Lint;
        opts.lint_paths.push_back(file);
        opts.lint_check = true;

        std::ostringstream out;
        std::ostringstream err;

        int res = CliDriver::run(opts, out, err);
        CHECK(res == 0);
        CHECK(err.str().empty());
    }
}
