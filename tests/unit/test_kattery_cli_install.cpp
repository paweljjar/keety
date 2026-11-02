#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/cli.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/process.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::pkg;
namespace fs = std::filesystem;

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

    fs::path createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath, std::ios::binary);
        ofs << content;
        return fullPath;
    }
};

} // namespace

TEST_CASE("Cli: kattery install, update, and list", "[kattery][cli]") {
    TempDirectory temp("kattery_cli_install");

    // Create local dependency
    temp.createFile("deps/liba/index.keet", "export let a = 1;");
    temp.createFile("deps/liba/pedigree.json", R"({"name":"liba","version":"1.0.0"})");

    // Create project manifest
    temp.createFile("pedigree.json", R"({
  "name": "app",
  "version": "1.0.0",
  "dependencies": {
    "liba": "path:./deps/liba"
  }
})");

    std::ostringstream out, err;

    SECTION("kattery install creates keet_modules and pedigree.lock") {
        int code = Cli::run({"install", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "keet_modules" / "liba" / "index.keet"));
        REQUIRE(fs::exists(temp.path / "pedigree.lock"));
    }

    SECTION("kattery i alias") {
        int code = Cli::run({"i", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "keet_modules" / "liba" / "index.keet"));
    }

    SECTION("kattery install --frozen succeeds when lock matches") {
        REQUIRE(Cli::run({"install", "--cwd", temp.path.string()}, out, err) == 0);

        out.str(""); err.str("");
        int code = Cli::run({"install", "--frozen", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
    }

    SECTION("kattery list prints package details") {
        REQUIRE(Cli::run({"install", "--cwd", temp.path.string()}, out, err) == 0);

        out.str(""); err.str("");
        int code = Cli::run({"list", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(out.str().find("app@1.0.0") != std::string::npos);
        REQUIRE(out.str().find("liba") != std::string::npos);
    }

    SECTION("kattery update runs successfully") {
        REQUIRE(Cli::run({"install", "--cwd", temp.path.string()}, out, err) == 0);

        out.str(""); err.str("");
        int code = Cli::run({"update", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);
        REQUIRE(fs::exists(temp.path / "pedigree.lock"));
    }
}
