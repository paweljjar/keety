#include <catch2/catch_test_macros.hpp>
#include <keety/pkg/source_spec.hpp>
#include <keety/pkg/diagnostics.hpp>

using namespace keety::pkg;

TEST_CASE("Package name validation", "[kattery][pkg]") {
    SECTION("Valid names") {
        REQUIRE(isValidPackageName("mathx"));
        REQUIRE(isValidPackageName("a"));
        REQUIRE(isValidPackageName("my-package"));
        REQUIRE(isValidPackageName("my_package"));
        REQUIRE(isValidPackageName("pkg123"));
        REQUIRE(isValidPackageName("a-b_c-1"));
        REQUIRE_NOTHROW(validatePackageName("valid_pkg"));
    }

    SECTION("Invalid names") {
        // Empty
        REQUIRE_FALSE(isValidPackageName(""));
        REQUIRE_THROWS_AS(validatePackageName(""), PkgError);

        // Starts with uppercase, digit, or symbol
        REQUIRE_FALSE(isValidPackageName("MathX"));
        REQUIRE_FALSE(isValidPackageName("123pkg"));
        REQUIRE_FALSE(isValidPackageName("-pkg"));
        REQUIRE_FALSE(isValidPackageName("_pkg"));

        // Invalid characters
        REQUIRE_FALSE(isValidPackageName("pkg.name"));
        REQUIRE_FALSE(isValidPackageName("pkg/name"));
        REQUIRE_FALSE(isValidPackageName("pkg@name"));
        REQUIRE_FALSE(isValidPackageName("pkg name"));

        // Too long (>64 characters)
        std::string longName(65, 'a');
        REQUIRE_FALSE(isValidPackageName(longName));
        REQUIRE_THROWS_AS(validatePackageName(longName), PkgError);
    }

    SECTION("Collisions with KeetScript standard library modules") {
        REQUIRE_FALSE(isValidPackageName("io"));
        REQUIRE_FALSE(isValidPackageName("fs"));
        REQUIRE_FALSE(isValidPackageName("sys"));
        REQUIRE_FALSE(isValidPackageName("math"));
        REQUIRE_FALSE(isValidPackageName("time"));
        REQUIRE_FALSE(isValidPackageName("json"));
        REQUIRE_FALSE(isValidPackageName("net"));
        REQUIRE_FALSE(isValidPackageName("crypto"));
        REQUIRE_FALSE(isValidPackageName("encoding"));
        REQUIRE_FALSE(isValidPackageName("test"));
        REQUIRE_FALSE(isValidPackageName("os"));
        REQUIRE_FALSE(isValidPackageName("std"));
        REQUIRE_FALSE(isValidPackageName("keet"));

        try {
            validatePackageName("net");
            FAIL("expected validatePackageName('net') to throw");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::InvalidPackageName);
            REQUIRE(err.diagnostic().message.find("collides") != std::string::npos);
        }
    }
}

TEST_CASE("Semver validation", "[kattery][pkg]") {
    SECTION("Valid semver") {
        REQUIRE(isValidSemver("0.0.0"));
        REQUIRE(isValidSemver("0.1.0"));
        REQUIRE(isValidSemver("1.0.0"));
        REQUIRE(isValidSemver("12.34.567"));
        REQUIRE_NOTHROW(validateSemver("1.0.0"));
    }

    SECTION("Invalid semver") {
        REQUIRE_FALSE(isValidSemver(""));
        REQUIRE_FALSE(isValidSemver("1"));
        REQUIRE_FALSE(isValidSemver("1.0"));
        REQUIRE_FALSE(isValidSemver("1.0.0.0"));
        REQUIRE_FALSE(isValidSemver("v1.0.0"));
        REQUIRE_FALSE(isValidSemver("01.0.0")); // Leading zeros disallowed
        REQUIRE_FALSE(isValidSemver("1.02.0"));
        REQUIRE_FALSE(isValidSemver("1.0.03"));
        REQUIRE_FALSE(isValidSemver("1.0.a"));
        REQUIRE_FALSE(isValidSemver("1.-1.0"));

        try {
            validateSemver("invalid");
            FAIL("expected validateSemver to throw");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::InvalidVersion);
        }
    }
}

TEST_CASE("Keety constraint validation", "[kattery][pkg]") {
    SECTION("Valid constraints") {
        REQUIRE(isValidKeetyConstraint(">=0.1.0"));
        REQUIRE(isValidKeetyConstraint(">=1.0.0"));
        REQUIRE(isValidKeetyConstraint(">=2.15.3"));
        REQUIRE_NOTHROW(validateKeetyConstraint(">=0.1.0"));
    }

    SECTION("Invalid constraints") {
        REQUIRE_FALSE(isValidKeetyConstraint(""));
        REQUIRE_FALSE(isValidKeetyConstraint("0.1.0"));
        REQUIRE_FALSE(isValidKeetyConstraint(">0.1.0"));
        REQUIRE_FALSE(isValidKeetyConstraint("^0.1.0"));
        REQUIRE_FALSE(isValidKeetyConstraint(">=0.1"));
        REQUIRE_FALSE(isValidKeetyConstraint(">=v0.1.0"));

        try {
            validateKeetyConstraint("0.1.0");
            FAIL("expected validateKeetyConstraint to throw");
        } catch (const PkgError& err) {
            REQUIRE(err.code() == ErrorCode::InvalidKeetyConstraint);
        }
    }
}

TEST_CASE("SourceSpec parsing and formatting", "[kattery][pkg]") {
    SECTION("Git URLs with tags and branches") {
        auto s1 = SourceSpec::parse("git+https://github.com/user/mathx.git#v1.2.0");
        REQUIRE(s1.isGit());
        REQUIRE(s1.url == "https://github.com/user/mathx.git");
        REQUIRE(s1.ref == "v1.2.0");
        REQUIRE(s1.toString() == "git+https://github.com/user/mathx.git#v1.2.0");

        auto s2 = SourceSpec::parse("git+https://github.com/user/mathx.git");
        REQUIRE(s2.isGit());
        REQUIRE(s2.url == "https://github.com/user/mathx.git");
        REQUIRE(s2.ref.empty());
        REQUIRE(s2.toString() == "git+https://github.com/user/mathx.git");

        auto s3 = SourceSpec::parse("git+git@github.com:user/mathx.git#main");
        REQUIRE(s3.isGit());
        REQUIRE(s3.url == "git@github.com:user/mathx.git");
        REQUIRE(s3.ref == "main");
        REQUIRE(s3.toString() == "git+git@github.com:user/mathx.git#main");
    }

    SECTION("Bare Git URLs auto-normalized") {
        auto s1 = SourceSpec::parse("https://github.com/user/repo.git#v1.0.0");
        REQUIRE(s1.isGit());
        REQUIRE(s1.url == "https://github.com/user/repo.git");
        REQUIRE(s1.ref == "v1.0.0");
        REQUIRE(s1.toString() == "git+https://github.com/user/repo.git#v1.0.0");

        auto s2 = SourceSpec::parse("git@github.com:user/repo.git");
        REQUIRE(s2.isGit());
        REQUIRE(s2.url == "git@github.com:user/repo.git");
        REQUIRE(s2.ref.empty());
    }

    SECTION("Local path specs") {
        auto p1 = SourceSpec::parse("path:../utils");
        REQUIRE(p1.isPath());
        REQUIRE(p1.path == "../utils");
        REQUIRE(p1.toString() == "path:../utils");

        // Backslashes are normalized to forward slashes
        auto p2 = SourceSpec::parse("path:..\\vendor\\foo");
        REQUIRE(p2.isPath());
        REQUIRE(p2.path == "../vendor/foo");
        REQUIRE(p2.toString() == "path:../vendor/foo");
    }

    SECTION("Invalid source specs") {
        REQUIRE_THROWS_AS(SourceSpec::parse(""), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("path:"), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("git+"), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("git+#v1.0"), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("git+https://github.com/u/p.git#"), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("ftp://example.com/repo.git"), PkgError);
        REQUIRE_THROWS_AS(SourceSpec::parse("foo/bar"), PkgError);
    }
}
