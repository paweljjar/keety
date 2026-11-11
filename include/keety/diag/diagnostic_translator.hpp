#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/error_code.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/diag/source_map.hpp>
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

/// Structured unresolved symbol parsed from host linker stdout/stderr
struct LinkerSymbol {
    std::string rawSymbol;          ///< Raw symbol as reported by linker (e.g. "_sqlite3_open")
    std::string demangledSymbol;    ///< Demangled and sanitized symbol (e.g. "sqlite3_open")
    std::string referencedFrom;     ///< Caller, function or object file referencing the symbol
    std::string suggestedPackage;   ///< Suggested native package/library name (e.g. "sqlite")
};

/// Translates and sanitizes raw host C++ toolchain diagnostics into KeetScript semantics
class DiagnosticTranslator {
public:
    /// Demangles raw C++ types (e.g. std::__1::vector<int64_t> -> [int])
    [[nodiscard]] static std::string demangle_cpp_type(std::string_view cpp_type);

    /// Demangles C++ or ABI mangled symbol names
    [[nodiscard]] static std::string demangle_symbol(std::string_view mangled_name);

    /// Suggests an originating native package or library for an unresolved symbol name
    [[nodiscard]] static std::string suggest_package_for_symbol(std::string_view symbol);

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

    /// Parses unresolved symbols and missing libraries from linker output (ld64, GNU ld, MSVC link.exe)
    [[nodiscard]] static std::vector<LinkerSymbol> parse_linker_output(std::string_view raw_output);

    /// Parses compiler diagnostic output based on toolchain kind
    [[nodiscard]] static std::vector<RawDiagnostic> parse_compiler_output(
        std::string_view raw_output,
        driver::CompilerKind kind = driver::CompilerKind::Clang);

    /// Translates raw compiler output into a list of native KeetScript Diagnostic objects
    [[nodiscard]] static std::vector<Diagnostic> translate(
        std::string_view raw_output,
        driver::CompilerKind kind = driver::CompilerKind::Clang,
        const SourceManager* source_mgr = nullptr,
        const SourceMap* source_map = nullptr);

    /// Translates raw linker output into a list of native KeetScript E6001 Diagnostic objects
    [[nodiscard]] static std::vector<Diagnostic> translate_linker_output(std::string_view raw_output);

    /// Maps a raw cleaned compiler error message to an appropriate KeetScript ErrorCode
    [[nodiscard]] static ErrorCode map_to_error_code(std::string_view clean_msg);
};

} // namespace keety::diag

