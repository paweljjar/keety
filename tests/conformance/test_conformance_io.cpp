#include <catch2/catch_test_macros.hpp>
#include <keety/driver/cli_driver.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <io.h>
#define DUP_FN _dup
#define DUP2_FN _dup2
#define FILENO_FN _fileno
#else
#include <fcntl.h>
#include <unistd.h>
#define DUP_FN dup
#define DUP2_FN dup2
#define FILENO_FN fileno
#endif

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

TEST_CASE("Positive conformance: Standard I/O module and console operations", "[conformance][io]") {
    ToolchainDriver driver;
    if (!driver.has_valid_toolchain()) {
        return;
    }

    SECTION("Compile and run io_console program with standard print and styling functions") {
        auto test_file = locate_conformance_test("positive/06_io/io_console.keet");
        REQUIRE(!test_file.empty());
        REQUIRE(std::filesystem::exists(test_file));

        auto temp_input = std::filesystem::temp_directory_path() / "keety_io_conformance_in.txt";
        {
            std::ofstream ofs(temp_input);
            ofs << "hello keetscript\nsample input\n";
        }

        int stdin_fd = FILENO_FN(stdin);
        int saved_stdin = DUP_FN(stdin_fd);
        FILE* redirected = std::freopen(temp_input.string().c_str(), "r", stdin);
        (void)redirected;

        std::ostringstream out;
        std::ostringstream err;
        auto opts = CliDriver::parse_args({"run", test_file.string()});

        int exit_code = CliDriver::run(opts, out, err);

        DUP2_FN(saved_stdin, stdin_fd);
#if defined(_WIN32)
        _close(saved_stdin);
#else
        close(saved_stdin);
#endif

        std::error_code ec;
        std::filesystem::remove(temp_input, ec);

        CHECK(exit_code == 0);
    }
}
