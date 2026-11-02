#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/workspace.hpp>
#include <keety/pkg/diagnostics.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

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

TEST_CASE("Workspace discovery and member expansion", "[kattery][workspace]") {
    TempDirectory temp("kattery_ws_test");

    // Create root pedigree.json
    temp.createFile("pedigree.json", R"({
  "name": "my-monorepo",
  "version": "1.0.0",
  "workspaces": [
    "packages/*",
    "apps/cli"
  ]
})");

    // Create member packages
    temp.createFile("packages/core/pedigree.json", R"({
  "name": "core",
  "version": "0.1.0"
})");

    temp.createFile("packages/utils/pedigree.json", R"({
  "name": "utils",
  "version": "0.2.0"
})");

    temp.createFile("apps/cli/pedigree.json", R"({
  "name": "cli",
  "version": "0.3.0"
})");

    SECTION("Discover from root directory") {
        auto ws = Workspace::discover(temp.path);
        REQUIRE(ws.rootDir == temp.path);
        REQUIRE(ws.rootManifest.name == "my-monorepo");
        REQUIRE(ws.hasMembers());
        REQUIRE(ws.members.size() == 3);

        // Members sorted lexicographically: cli, core, utils
        REQUIRE(ws.members[0].name == "cli");
        REQUIRE(ws.members[0].relativePath == "apps/cli");
        REQUIRE(ws.members[1].name == "core");
        REQUIRE(ws.members[1].relativePath == "packages/core");
        REQUIRE(ws.members[2].name == "utils");
        REQUIRE(ws.members[2].relativePath == "packages/utils");

        const auto* mCore = ws.findMember("core");
        REQUIRE(mCore != nullptr);
        REQUIRE(mCore->manifest.version == "0.1.0");

        REQUIRE(ws.findMember("nonexistent") == nullptr);
    }

    SECTION("Discover upward from nested member directory") {
        fs::path nested = temp.path / "packages" / "core" / "src";
        fs::create_directories(nested);

        auto ws = Workspace::discover(nested);
        REQUIRE(ws.rootDir == temp.path);
        REQUIRE(ws.rootManifest.name == "my-monorepo");
        REQUIRE(ws.members.size() == 3);

        const auto* found = ws.findMemberByPath(nested);
        REQUIRE(found != nullptr);
        REQUIRE(found->name == "core");
    }
}

TEST_CASE("Workspace error cases", "[kattery][workspace]") {
    SECTION("Member missing pedigree.json") {
        TempDirectory temp("kattery_ws_missing");
        temp.createFile("pedigree.json", R"({
  "name": "root-ws",
  "version": "1.0.0",
  "workspaces": ["apps/cli"]
})");
        fs::create_directories(temp.path / "apps" / "cli"); // Directory exists, but no pedigree.json

        try {
            (void)Workspace::discover(temp.path);
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::WorkspaceMemberMissing);
        }
    }

    SECTION("Duplicate member package name") {
        TempDirectory temp("kattery_ws_dup");
        temp.createFile("pedigree.json", R"({
  "name": "root-ws",
  "version": "1.0.0",
  "workspaces": ["packages/*"]
})");
        temp.createFile("packages/p1/pedigree.json", R"({"name":"duppkg","version":"1.0.0"})");
        temp.createFile("packages/p2/pedigree.json", R"({"name":"duppkg","version":"2.0.0"})");

        try {
            (void)Workspace::discover(temp.path);
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::DuplicateWorkspaceMember);
        }
    }

    SECTION("Member name collides with root name") {
        TempDirectory temp("kattery_ws_root_coll");
        temp.createFile("pedigree.json", R"({
  "name": "my-pkg",
  "version": "1.0.0",
  "workspaces": ["packages/*"]
})");
        temp.createFile("packages/p1/pedigree.json", R"({"name":"my-pkg","version":"1.0.0"})");

        try {
            (void)Workspace::discover(temp.path);
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::DuplicateWorkspaceMember);
        }
    }

    SECTION("Invalid non-trailing glob pattern") {
        TempDirectory temp("kattery_ws_invalid_glob");
        temp.createFile("pedigree.json", R"({
  "name": "my-pkg",
  "version": "1.0.0",
  "workspaces": ["foo/*/bar"]
})");

        try {
            (void)Workspace::discover(temp.path);
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidWorkspaceEntry);
        }
    }
}
