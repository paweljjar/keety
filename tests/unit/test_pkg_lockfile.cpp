#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/lockfile.hpp>
#include <keety/pkg/manifest.hpp>
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
};

} // namespace

TEST_CASE("Lockfile parsing: valid pedigree.lock", "[kattery][lockfile]") {
    const std::string json = R"({
  "lockVersion": 1,
  "packages": {
    "mathx": {
      "source": "git",
      "url": "https://github.com/user/mathx.git",
      "ref": "v1.2.0",
      "commit": "9fceb02d0ae598e95dc970b74767f19372d61af8",
      "tree": "4b825dc642cb6eb9a060e54bf8d69288fbee4904",
      "dependencies": ["strutil"]
    },
    "utils": {
      "source": "path",
      "path": "../utils",
      "dependencies": []
    }
  }
})";

    auto lock = Lockfile::parse(json);
    REQUIRE(lock.lockVersion == 1);
    REQUIRE(lock.packages.size() == 2);

    const auto& m = lock.packages.at("mathx");
    REQUIRE(m.isGit());
    REQUIRE(m.url == "https://github.com/user/mathx.git");
    REQUIRE(m.ref == "v1.2.0");
    REQUIRE(m.commit == "9fceb02d0ae598e95dc970b74767f19372d61af8");
    REQUIRE(m.tree == "4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    REQUIRE(m.dependencies.size() == 1);
    REQUIRE(m.dependencies[0] == "strutil");

    const auto& u = lock.packages.at("utils");
    REQUIRE(u.isPath());
    REQUIRE(u.path == "../utils");
    REQUIRE(u.dependencies.empty());
}

TEST_CASE("Lockfile validation errors", "[kattery][lockfile]") {
    SECTION("Unsupported lockVersion") {
        try {
            (void)Lockfile::parse(R"({"lockVersion": 2, "packages": {}})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::UnsupportedLockVersion);
        }
    }

    SECTION("Invalid commit SHA length") {
        try {
            (void)Lockfile::parse(R"({
  "lockVersion": 1,
  "packages": {
    "mathx": {
      "source": "git",
      "url": "https://github.com/u/m.git",
      "commit": "short_sha",
      "tree": "4b825dc642cb6eb9a060e54bf8d69288fbee4904"
    }
  }
})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidLockfile);
        }
    }

    SECTION("Non-hex characters in tree SHA") {
        try {
            (void)Lockfile::parse(R"({
  "lockVersion": 1,
  "packages": {
    "mathx": {
      "source": "git",
      "url": "https://github.com/u/m.git",
      "commit": "9fceb02d0ae598e95dc970b74767f19372d61af8",
      "tree": "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
    }
  }
})");
            FAIL("expected throw");
        } catch (const PkgError& e) {
            REQUIRE(e.code() == ErrorCode::InvalidLockfile);
        }
    }
}

TEST_CASE("Lockfile satisfies Manifest", "[kattery][lockfile]") {
    Manifest m;
    m.name = "app";
    m.version = "1.0.0";
    m.dependencies["mathx"] = SourceSpec::parse("git+https://github.com/user/mathx.git#v1.0.0");
    m.dependencies["utils"] = SourceSpec::parse("path:../utils");

    Lockfile lock;
    LockedPackage lmath;
    lmath.source = SourceKind::Git;
    lmath.url = "https://github.com/user/mathx.git";
    lmath.ref = "v1.0.0";
    lmath.commit = "9fceb02d0ae598e95dc970b74767f19372d61af8";
    lmath.tree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
    lock.packages["mathx"] = lmath;

    LockedPackage lutils;
    lutils.source = SourceKind::Path;
    lutils.path = "../utils";
    lock.packages["utils"] = lutils;

    SECTION("Satisfied when all dependencies match") {
        REQUIRE(lock.satisfies(m));
    }

    SECTION("Not satisfied when a dependency is missing") {
        Lockfile missingLock = lock;
        missingLock.packages.erase("utils");
        REQUIRE_FALSE(missingLock.satisfies(m));
    }

    SECTION("Not satisfied when ref differs") {
        Lockfile diffRefLock = lock;
        diffRefLock.packages["mathx"].ref = "v2.0.0";
        REQUIRE_FALSE(diffRefLock.satisfies(m));
    }

    SECTION("Not satisfied when path differs") {
        Lockfile diffPathLock = lock;
        diffPathLock.packages["utils"].path = "../other";
        REQUIRE_FALSE(diffPathLock.satisfies(m));
    }
}

TEST_CASE("Lockfile disk load and save", "[kattery][lockfile]") {
    TempDirectory temp("kattery_lockfile_test");
    fs::path target = temp.path / "pedigree.lock";

    Lockfile lock;
    lock.lockVersion = 1;
    LockedPackage p;
    p.source = SourceKind::Path;
    p.path = "./local";
    lock.packages["localpkg"] = p;

    lock.save(target);
    REQUIRE(fs::exists(target));

    Lockfile loaded = Lockfile::load(target);
    REQUIRE(loaded.lockVersion == 1);
    REQUIRE(loaded.packages.size() == 1);
    REQUIRE(loaded.packages.at("localpkg").isPath());
    REQUIRE(loaded.packages.at("localpkg").path == "./local");
}
