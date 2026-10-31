#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/driver/toolchain_driver.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::driver {

/// Top-level CLI command kinds
enum class CliCommand {
    Check,   ///< keety check <file> (IDE-grade validation)
    Emit,    ///< keety emit <file> (--cpp | --hpp | --ast | --ir)
    Build,   ///< keety build <file>
    Run,     ///< keety run <file> [-- args]
    Test,    ///< keety test [--benchmarks | --conformance]
    Version, ///< keety --version / -V
    Help,    ///< keety --help / -h
    Unknown  ///< Unrecognized command
};

[[nodiscard]] std::string_view to_string(CliCommand cmd) noexcept;

/// Intermediate or emission target
enum class EmitTarget {
    None,
    Cpp,
    Hpp,
    Ast,
    Ir
};

[[nodiscard]] std::string_view to_string(EmitTarget target) noexcept;

/// Parsed CLI configuration options
struct CliOptions {
    CliCommand command{CliCommand::Unknown};
    std::filesystem::path input_file{};
    EmitTarget emit_target{EmitTarget::None};
    std::filesystem::path output_dir{};
    bool output_is_dir{false};
    std::string cxx_compiler{};
    OptimizationLevel opt_level{OptimizationLevel::O3};
    bool wrapv{true};
    bool verbose{false};
    bool test_conformance{false};
    bool test_benchmarks{false};
    std::vector<std::string> run_args{};
    diag::ColorMode color_mode{diag::ColorMode::Never};
};

/// CLI command driver and entry point
class CliDriver {
public:
    /// Parses argc/argv into structured CLI options
    [[nodiscard]] static CliOptions parse_args(int argc, char* argv[]);

    /// Parses string arguments list into structured CLI options
    [[nodiscard]] static CliOptions parse_args(const std::vector<std::string>& args);

    /// Main entry execution dispatcher
    static int run(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Overload taking argc and argv directly
    static int run(int argc, char* argv[], std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Executes `keety check <file>` validation pipeline
    static int execute_check(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Executes `keety emit <file> (--cpp | --hpp | --ast | --ir)` emission pipeline
    static int execute_emit(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Executes `keety build <file>` compilation and native linking pipeline
    static int execute_build(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Executes `keety run <file> [-- args]` compilation, linking, and execution pipeline
    static int execute_run(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Executes `keety test [--benchmarks | --conformance]` test and benchmark suite
    static int execute_test(const CliOptions& options, std::ostream& out = std::cout, std::ostream& err = std::cerr);

    /// Helper to compile and link to a binary, returning target binary path if successful
    static std::optional<std::filesystem::path> build_binary(const CliOptions& options, std::ostream& out, std::ostream& err);

    /// Prints CLI help documentation
    static void print_help(std::ostream& out);

    /// Prints CLI version information
    static void print_version(std::ostream& out);
};

} // namespace keety::driver
