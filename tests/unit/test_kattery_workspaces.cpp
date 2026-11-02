#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/cli.hpp>
#include <keety/pkg/manifest.hpp>
#include <keety/pkg/lockfile.hpp>

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

TEST_CASE("Cli: Monorepo workspace operations", "[kattery][cli][workspace]") {
    TempDirectory temp("kattery_ws_cli");

    // Root manifest
    temp.createFile("pedigree.json", R"({
  "name": "my-monorepo",
  "version": "1.0.0",
  "workspaces": [
    "packages/*"
  ]
})");

    // Workspace member packages
    temp.createFile("packages/core/pedigree.json", R"({"name":"core","version":"0.1.0"})");
    temp.createFile("packages/core/index.keet", "export let core = 1;");

    temp.createFile("packages/ui/pedigree.json", R"({"name":"ui","version":"0.1.0"})");
    temp.createFile("packages/ui/index.keet", "export let ui = 2;");

    // External local library
    temp.createFile("vendor/fastmath/pedigree.json", R"({"name":"fastmath","version":"2.0.0"})");
    temp.createFile("vendor/fastmath/index.keet", "export let calc = 42;");

    std::ostringstream out, err;

    SECTION("kattery install links workspace members into root keet_modules") {
        int code = Cli::run({"install", "--cwd", temp.path.string()}, out, err);
        REQUIRE(code == 0);

        REQUIRE(fs::exists(temp.path / "keet_modules" / "core" / "index.keet"));
        REQUIRE(fs::exists(temp.path / "keet_modules" / "ui" / "index.keet"));
    }

    SECTION("kattery add -w <member> targets specific workspace member") {
        int code = Cli::run({
            "add", "fastmath", "path:../../vendor/fastmath",
            "-w", "core",
            "--cwd", temp.path.string()
        }, out, err);
        REQUIRE(code == 0);

        // Core member manifest now records fastmath
        Manifest coreM = Manifest::load(temp.path / "packages" / "core" / "pedigree.json");
        REQUIRE(coreM.dependencies.find("fastmath") != coreM.dependencies.end());

        // Fastmath is hoisted to root keet_modules/
        REQUIRE(fs::exists(temp.path / "keet_modules" / "fastmath" / "index.keet"));

        // UI member manifest does not have fastmath
        Manifest uiM = Manifest::load(temp.path / "packages" / "ui" / "pedigree.json");
        REQUIRE(uiM.dependencies.find("fastmath") == uiM.dependencies.end());
    }

    SECTION("kattery remove -w <member> targets specific workspace member") {
        // First add to core
        REQUIRE(Cli::run({
            "add", "fastmath", "path:../../vendor/fastmath",
            "-w", "core",
            "--cwd", temp.path.string()
        }, out, err) == 0);

        // Remove from core
        out.str(""); err.str("");
        int code = Cli::run({
            "remove", "fastmath",
            "-w", "core",
            "--cwd", temp.path.string()
        }, out, err);
        REQUIRE(code == 0);

        Manifest coreM = Manifest::load(temp.path / "packages" / "core" / "pedigree.json");
        REQUIRE_FALSE(coreM.hasDependency("fastmath"));
        REQUIRE_FALSE(fs::exists(temp.path / "keet_modules" / "fastmath"));
    }
}
