#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>
#include <keety/pkg/cli.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace keety::driver;
using namespace keety::pkg;

namespace {

struct TempDirectory {
    fs::path path;

    explicit TempDirectory(const std::string& prefix) {
        path = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << content;
    }
};

} // namespace

TEST_CASE("Conformance: Keety compiles and runs programs importing packages installed via kattery", "[conformance][pkg][import]") {
    ToolchainDriver toolchain;
    if (!toolchain.has_valid_toolchain()) {
        return;
    }

    TempDirectory temp("test_pkg_import");

    // 1. Create a local package 'mathx' with index.keet
    temp.createFile("packages/mathx/pedigree.json", R"({
  "name": "mathx",
  "version": "1.0.0"
})");
    temp.createFile("packages/mathx/index.keet", R"(
export fn add(a: int, b: int): int {
    return a + b;
}
)");

    // 2. Initialize consumer project and adopt local package via kattery CLI
    temp.createFile("app/pedigree.json", R"({
  "name": "calc-app",
  "version": "1.0.0",
  "entry": "src/main.keet"
})");

    fs::path appDir = temp.path / "app";
    fs::path mathxDir = temp.path / "packages" / "mathx";

    std::ostringstream katOut;
    std::ostringstream katErr;
    int addCode = Cli::run({
        "--cwd", appDir.string(),
        "add", "mathx", "path:" + mathxDir.string()
    }, katOut, katErr);

    REQUIRE(addCode == 0);
    REQUIRE(fs::exists(appDir / "keet_modules" / "mathx" / "index.keet"));

    // 3. Create consumer entry file importing 'mathx'
    temp.createFile("app/src/main.keet", R"(
import io;
import mathx;

fn main(): void {
    let result: int = mathx.add(40, 2);
    io.println(result);
}
)");

    // 4. Run the consumer program with keety compiler pipeline
    fs::path entryFile = appDir / "src" / "main.keet";
    std::ostringstream runOut;
    std::ostringstream runErr;

    auto opts = CliDriver::parse_args({"run", entryFile.string()});
    int runCode = CliDriver::run(opts, runOut, runErr);

    CHECK(runCode == 0);
}
