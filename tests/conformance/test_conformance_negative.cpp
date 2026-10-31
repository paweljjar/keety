#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

#include <filesystem>
#include <sstream>
#include <string>

using namespace keety::driver;

namespace {

std::filesystem::path locate_conformance_test(const std::string& rel_path) {
    std::filesystem::path curr = std::filesystem::current_path();
    for (int i = 0; i < 5; ++i) {
        auto candidate = curr / "tests" / "conformance" / rel_path;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!curr.has_parent_path() || curr.parent_path() == curr) break;
        curr = curr.parent_path();
    }
    return "";
}

} // namespace

TEST_CASE("Negative compiler rejection conformance: Diagnostic error codes", "[conformance][negative]") {
    SECTION("Reject lexical identifier shadowing with E2001 in neg_shadowing") {
        auto test_file = locate_conformance_test("negative/neg_shadowing.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E2001") != std::string::npos);
        CHECK(err.str().find("error[E2001]") != std::string::npos);
    }

    SECTION("Reject duplicate mutable reference aliasing with E4001 in neg_mut_alias") {
        auto test_file = locate_conformance_test("negative/neg_mut_alias.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E4001") != std::string::npos);
        CHECK(err.str().find("error[E4001]") != std::string::npos);

        // Verify compiler also rejects build command
        std::ostringstream b_out;
        std::ostringstream b_err;
        auto b_opts = CliDriver::parse_args({"build", test_file.string()});
        int b_code = CliDriver::run(b_opts, b_out, b_err);
        CHECK(b_code == 1);
        CHECK(b_err.str().find("E4001") != std::string::npos);
    }

    SECTION("Reject circular module dependency cycle with E1001 in neg_circular_module") {
        auto test_file = locate_conformance_test("negative/neg_circular_module_a.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E1001") != std::string::npos);
        CHECK(err.str().find("error[E1001]") != std::string::npos);
    }

    SECTION("Reject implicit numeric widening zero-coercion violation with E3002 in neg_implicit_widening") {
        auto test_file = locate_conformance_test("negative/neg_implicit_widening.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E3002") != std::string::npos);
        CHECK(err.str().find("error[E3002]") != std::string::npos);
    }

    SECTION("Reject simultaneous container and element mutable borrow with E4002 in neg_mut_container_elem") {
        auto test_file = locate_conformance_test("negative/neg_mut_container_elem.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E4002") != std::string::npos);
        CHECK(err.str().find("error[E4002]") != std::string::npos);
    }

    SECTION("Reject missing return statement in non-void function with E5001 in neg_missing_return") {
        auto test_file = locate_conformance_test("negative/neg_missing_return.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E5001") != std::string::npos);
        CHECK(err.str().find("error[E5001]") != std::string::npos);
    }

    SECTION("Reject assignment to immutable constant with E2005 in neg_const_assign") {
        auto test_file = locate_conformance_test("negative/neg_const_assign.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E2005") != std::string::npos);
        CHECK(err.str().find("error[E2005]") != std::string::npos);
    }

    SECTION("Reject map element mutable reference borrow with E4003 in neg_map_mut") {
        auto test_file = locate_conformance_test("negative/neg_map_mut.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E4003") != std::string::npos);
        CHECK(err.str().find("error[E4003]") != std::string::npos);
    }

    SECTION("Reject mutable reference parameter in async function with E4004 in neg_async_mut") {
        auto test_file = locate_conformance_test("negative/neg_async_mut.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E4004") != std::string::npos);
        CHECK(err.str().find("error[E4004]") != std::string::npos);
    }

    SECTION("Reject variable declaration without explicit type annotation with E3001 in neg_missing_type") {
        auto test_file = locate_conformance_test("negative/neg_missing_type.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E3001") != std::string::npos);
        CHECK(err.str().find("error[E3001]") != std::string::npos);
    }

    SECTION("Reject writing to imported export let const-view with E2002 in neg_export_const_write") {
        auto test_file = locate_conformance_test("negative/neg_export_const_write.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E2002") != std::string::npos);
        CHECK(err.str().find("error[E2002]") != std::string::npos);
    }

    SECTION("Reject simultaneous container and element mutable borrow with E4002 in neg_container_elem") {
        auto test_file = locate_conformance_test("negative/neg_container_elem.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E4002") != std::string::npos);
        CHECK(err.str().find("error[E4002]") != std::string::npos);
    }

    SECTION("Reject argument type mismatch in standard io call with E3003 in neg_io_type_mismatch") {
        auto test_file = locate_conformance_test("negative/neg_io_type_mismatch.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E3003") != std::string::npos);
        CHECK(err.str().find("error[E3003]") != std::string::npos);
    }

    SECTION("Reject argument type mismatch in standard fs call with E3003 in neg_fs_type_mismatch") {
        auto test_file = locate_conformance_test("negative/neg_fs_type_mismatch.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"check", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);
        CHECK(exit_code == 1);
        CHECK(err.str().find("E3003") != std::string::npos);
        CHECK(err.str().find("error[E3003]") != std::string::npos);
    }
}

