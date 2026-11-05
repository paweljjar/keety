#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/source_location.hpp>

#include <optional>
#include <string>
#include <vector>

namespace keety::lint {

/// Represents a source code text replacement produced by an automated quick fix.
struct FixEdit {
    diag::SourceSpan span;
    std::string replacement;
};

/// Represents an individual lint violation produced by a LintRule.
struct LintDiagnostic {
    std::string ruleCode;              ///< Deterministic rule code (e.g. "L001", "L010")
    std::string ruleName;              ///< Canonical rule identifier (e.g. "unused-import")
    diag::DiagnosticSeverity severity; ///< Error, Warning, Note/Info
    std::string message;               ///< Human-readable diagnostic description
    diag::SourceSpan span;             ///< Primary source span for squiggly highlighting
    std::optional<FixEdit> quickFix;   ///< Optional machine-applicable fix
    std::vector<diag::DiagnosticFooter> footers; ///< Contextual hints or suggestions

    LintDiagnostic() = default;

    LintDiagnostic(
        std::string code,
        std::string name,
        diag::DiagnosticSeverity sev,
        std::string msg,
        diag::SourceSpan sp,
        std::optional<FixEdit> fix = std::nullopt
    ) : ruleCode(std::move(code)),
        ruleName(std::move(name)),
        severity(sev),
        message(std::move(msg)),
        span(sp),
        quickFix(std::move(fix)) {}

    /// Converts into a standard compiler Diagnostic object.
    [[nodiscard]] diag::Diagnostic toCompilerDiagnostic() const {
        diag::Diagnostic d;
        d.severity = severity;
        d.code = ruleCode;
        d.message = message;
        d.location = span.start;
        d.withLabel(span, message, true);
        for (const auto& footer : footers) {
            d.footers.push_back(footer);
        }
        return d;
    }
};

} // namespace keety::lint
