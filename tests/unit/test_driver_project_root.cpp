#include <catch2/catch_test_macros.hpp>
#include <keety/driver/project_root.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

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

    void createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << content;
    }

    void createDir(const fs::path& relPath) {
        fs::create_directories(path / relPath);
    }
};

} // namespace

TEST_CASE("driver::findProjectRoot discovers nearest project root", "[driver][project_root]") {
    TempDirectory temp("test_project_root");

    SECTION("Nearest ancestor with keet_modules takes priority") {
        temp.createDir("project/keet_modules");
        temp.createFile("project/src/sub/entry.keet", "fun main(): void {}");

        fs::path entry = temp.path / "project" / "src" / "sub" / "entry.keet";
        fs::path root = keety::driver::findProjectRoot(entry);

        REQUIRE(fs::equivalent(root, temp.path / "project"));
    }

    SECTION("Nearest ancestor with pedigree.json found when no keet_modules exists") {
        temp.createFile("project/pedigree.json", "{}");
        temp.createFile("project/src/entry.keet", "fun main(): void {}");

        fs::path entry = temp.path / "project" / "src" / "entry.keet";
        fs::path root = keety::driver::findProjectRoot(entry);

        REQUIRE(fs::equivalent(root, temp.path / "project"));
    }

    SECTION("Workspace member: root with keet_modules wins over member with pedigree.json") {
        // Root has keet_modules where hoisted packages live
        temp.createDir("workspace_root/keet_modules");
        temp.createFile("workspace_root/pedigree.json", "{\"workspaces\": [\"packages/*\"]}");

        // Member package has pedigree.json but no keet_modules
        temp.createFile("workspace_root/packages/member_a/pedigree.json", "{\"name\": \"member_a\"}");
        temp.createFile("workspace_root/packages/member_a/src/main.keet", "fun main(): void {}");

        fs::path entry = temp.path / "workspace_root" / "packages" / "member_a" / "src" / "main.keet";
        fs::path root = keety::driver::findProjectRoot(entry);

        REQUIRE(fs::equivalent(root, temp.path / "workspace_root"));
    }

    SECTION("Fallback to entry file's parent directory when neither exists") {
        temp.createFile("isolated/deep/nested/standalone.keet", "fun main(): void {}");

        fs::path entry = temp.path / "isolated" / "deep" / "nested" / "standalone.keet";
        fs::path root = keety::driver::findProjectRoot(entry);

        REQUIRE(fs::equivalent(root, temp.path / "isolated" / "deep" / "nested"));
    }

    SECTION("Fallback when entry is a relative filename without parent path") {
        fs::path root = keety::driver::findProjectRoot("main.keet");
        REQUIRE(fs::equivalent(root, fs::current_path()));
    }
}

TEST_CASE("driver::resolveBuildDirectory discovers buildDir from pedigree.json", "[driver][project_root]") {
    TempDirectory temp("test_resolve_build_dir");

    SECTION("Discovers buildDir when pedigree.json specifies buildDir") {
        temp.createFile("project/pedigree.json", "{\"name\":\"app\",\"version\":\"0.1.0\",\"buildDir\":\"build\"}");
        temp.createFile("project/src/main.keet", "fn main(): void {}");

        fs::path entry = temp.path / "project" / "src" / "main.keet";
        auto buildDirOpt = keety::driver::resolveBuildDirectory(entry);

        REQUIRE(buildDirOpt.has_value());
        REQUIRE(*buildDirOpt == (temp.path / "project" / "build").lexically_normal());
    }

    SECTION("Discovers custom buildDir like dist/bin") {
        temp.createFile("project/pedigree.json", "{\"name\":\"app\",\"version\":\"0.1.0\",\"buildDir\":\"dist/bin\"}");
        temp.createFile("project/src/sub/main.keet", "fn main(): void {}");

        fs::path entry = temp.path / "project" / "src" / "sub" / "main.keet";
        auto buildDirOpt = keety::driver::resolveBuildDirectory(entry);

        REQUIRE(buildDirOpt.has_value());
        REQUIRE(*buildDirOpt == (temp.path / "project" / "dist" / "bin").lexically_normal());
    }

    SECTION("Returns nullopt when pedigree.json does not specify buildDir") {
        temp.createFile("project/pedigree.json", "{\"name\":\"app\",\"version\":\"0.1.0\"}");
        temp.createFile("project/src/main.keet", "fn main(): void {}");

        fs::path entry = temp.path / "project" / "src" / "main.keet";
        auto buildDirOpt = keety::driver::resolveBuildDirectory(entry);

        REQUIRE_FALSE(buildDirOpt.has_value());
    }

    SECTION("Returns nullopt when no pedigree.json exists") {
        temp.createFile("standalone/src/main.keet", "fn main(): void {}");

        fs::path entry = temp.path / "standalone" / "src" / "main.keet";
        auto buildDirOpt = keety::driver::resolveBuildDirectory(entry);

        REQUIRE_FALSE(buildDirOpt.has_value());
    }
}
