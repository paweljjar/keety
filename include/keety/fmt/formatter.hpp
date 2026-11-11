#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/ast/type.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/fmt/comment.hpp>
#include <keety/fmt/formatter_options.hpp>
#include <keety/parser/parser.hpp>

#include <string>
#include <string_view>

namespace keety::fmt {

/// Represents a range of differing lines between original and formatted source code.
struct LineDiffChunk {
    size_t origStart = 0;
    size_t origEnd = 0; // [origStart, origEnd)
    size_t fmtStart = 0;
    size_t fmtEnd = 0;  // [fmtStart, fmtEnd)
};

class Formatter {
public:
    explicit Formatter(FormatterOptions options = {}) : options_(options) {}

    /// Formats an entire KeetScript source file string into canonical formatted code.
    [[nodiscard]] std::string format(
        std::string_view source,
        std::string_view filename = "<input>",
        diag::DiagnosticEmitter* emitter = nullptr
    );

    /// Formats a specific range of lines in source code, applying formatted changes only within [startLine, endLine].
    [[nodiscard]] std::string formatRange(
        std::string_view source,
        size_t startLine,
        size_t endLine,
        std::string_view filename = "<input>",
        diag::DiagnosticEmitter* emitter = nullptr
    );

    /// Computes line-level diff chunks between original and formatted lines using LCS.
    [[nodiscard]] static std::vector<LineDiffChunk> computeLineDiff(
        const std::vector<std::string>& orig,
        const std::vector<std::string>& fmt
    );

    /// Formats a parsed ModuleAST with its associated CommentMap into canonical formatted code.
    [[nodiscard]] std::string formatModule(
        const parser::ModuleAST& moduleAst,
        const CommentMap& commentMap
    );

    /// Generates unified diff between original and formatted source code.
    [[nodiscard]] static std::string generateDiff(
        std::string_view original,
        std::string_view formatted,
        std::string_view filepath = "file.keet"
    );

    [[nodiscard]] const FormatterOptions& getOptions() const noexcept { return options_; }

private:
    FormatterOptions options_;
};

} // namespace keety::fmt
