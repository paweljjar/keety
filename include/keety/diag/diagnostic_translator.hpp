#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/error_code.hpp>
#include <keety/driver/toolchain_detector.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::diag {

/// Structured intermediate diagnostic parsed directly from raw compiler stdout/stderr
struct RawDiagnostic {
    std::string file;
    uint32_t line{0};
    uint32_t column{0};
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string code;
    std::string rawMessage;
    std::string cleanMessage;
    std::vector<std::string> notes;

    [[nodiscard]] bool isError() const noexcept {
        return severity == DiagnosticSeverity::Error;
    }
};

/// Translates and sanitizes raw host C++ toolchain diagnostics into KeetScript semantics
class DiagnosticTranslator {
public:
    /// Demangles raw C++ types (e.g. std::__1::vector<int64_t> -> [int])
    [[nodiscard]] static std::string demangle_cpp_type(std::string_view cpp_type);

    /// Cleans an entire error message, demangling embedded C++ types and stripping internal symbols
    [[nodiscard]] static std::string clean_diagnostic_message(std::string_view raw_msg);

    /// Checks if a file path belongs to compiler internal or C++ runtime headers
    [[nodiscard]] static bool is_internal_header(std::string_view file_path);

    /// Checks if a diagnostic line is C++ STL template noise (e.g. candidate function backtraces)
    [[nodiscard]] static bool is_internal_noise_line(std::string_view line);

    /// Parses raw diagnostic output from Clang or GCC compilers
    [[nodiscard]] static std::vector<RawDiagnostic> parse_clang_gcc_output(std::string_view raw_output);

    /// Parses raw diagnostic output from Microsoft Visual C++ (MSVC / cl.exe)
    [[nodiscard]] static std::vector<RawDiagnostic> parse_msvc_output(std::string_view raw_output);

    /// Parses compiler diagnostic output based on toolchain kind
    [[nodiscard]] static std::vector<RawDiagnostic> parse_compiler_output(
        std::string_view raw_output,
        driver::CompilerKind kind = driver::CompilerKind::Clang);

    /// Translates raw compiler output into a list of native KeetScript Diagnostic objects
    [[nodiscard]] static std::vector<Diagnostic> translate(
        std::string_view raw_output,
        driver::CompilerKind kind = driver::CompilerKind::Clang);

    /// Maps a raw cleaned compiler error message to an appropriate KeetScript ErrorCode
    [[nodiscard]] static ErrorCode map_to_error_code(std::string_view clean_msg);
};

} // namespace keety::diag

