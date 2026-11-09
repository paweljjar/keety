#pragma once

#include <keety/lint/lint_rule.hpp>

namespace keety::lint {

/// Lint rule that enforces UI hygiene in KML templates:
/// - Warns on unkeyed `{#for}` blocks over dynamic collections
/// - Flags forbidden C++ `::` syntax in KeetScript expressions, pointing to the canonical dot `.` syntax
/// - Detects invalid directive syntax (e.g. attempting to use `:` instead of `@` or `$`)
class KmlHygieneRule : public LintRule {
public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "kml-hygiene";
    }

    [[nodiscard]] std::string_view getCode() const noexcept override {
        return "L030";
    }

    [[nodiscard]] std::string_view getDescription() const noexcept override {
        return "Enforces reactive UI hygiene: keyed for-loops, canonical dot syntax, and valid directive sigils";
    }

    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Warning;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext& ctx) override {
        std::vector<LintDiagnostic> diagnostics;
        std::string_view src = ctx.sourceCode;
        if (src.empty()) return diagnostics;
        diag::FileId fid = (ctx.moduleAst.fileId != diag::INVALID_FILE_ID) ? ctx.moduleAst.fileId : 1;

        auto fillSpan = [&](diag::SourceSpan& span, size_t startOff, size_t endOff) {
            span.start.fileId = fid;
            span.end.fileId = fid;
            span.start.offset = static_cast<uint32_t>(startOff);
            span.end.offset = static_cast<uint32_t>(endOff);
            span.start.line = 1;
            span.start.column = 1;
            for (size_t i = 0; i < startOff && i < src.size(); ++i) {
                if (src[i] == '\n') {
                    span.start.line++;
                    span.start.column = 1;
                } else {
                    span.start.column++;
                }
            }
            span.end.line = span.start.line;
            span.end.column = span.start.column;
            for (size_t i = startOff; i < endOff && i < src.size(); ++i) {
                if (src[i] == '\n') {
                    span.end.line++;
                    span.end.column = 1;
                } else {
                    span.end.column++;
                }
            }
        };

        // 1. Check for unkeyed {#for} blocks
        size_t forPos = 0;
        while ((forPos = src.find("{#for", forPos)) != std::string_view::npos) {
            size_t endBlock = src.find('}', forPos);
            if (endBlock != std::string_view::npos) {
                std::string_view forHeader = src.substr(forPos, endBlock - forPos);
                // If it doesn't contain ';' or 'key=', emit warning
                if (forHeader.find(';') == std::string_view::npos && forHeader.find("key=") == std::string_view::npos) {
                    LintDiagnostic d;
                    d.ruleName = getName();
                    d.ruleCode = getCode();
                    d.severity = getDefaultSeverity();
                    d.message = "Unkeyed `{#for}` block over dynamic collection; provide a key expression (e.g. `{#for (item in items; key=item.id)}`) to preserve DOM identity";
                    fillSpan(d.span, forPos, endBlock + 1);
                    diagnostics.push_back(std::move(d));
                }
            }
            forPos += 5;
        }

        // 2. Check for forbidden C++ `::` syntax
        size_t colonColon = 0;
        while ((colonColon = src.find("::", colonColon)) != std::string_view::npos) {
            LintDiagnostic d;
            d.ruleName = getName();
            d.ruleCode = "L031";
            d.severity = diag::DiagnosticSeverity::Error;
            d.message = "Forbidden C++ `::` syntax used; KeetScript uses canonical dot `.` syntax for module namespaces and enum members";
            fillSpan(d.span, colonColon, colonColon + 2);
            diagnostics.push_back(std::move(d));
            colonColon += 2;
        }

        // 3. Catch invalid directive syntax: e.g. `:click=` or `:bind=`
        size_t badDir = 0;
        while ((badDir = src.find(":click=", badDir)) != std::string_view::npos) {
            LintDiagnostic d;
            d.ruleName = getName();
            d.ruleCode = "L032";
            d.severity = diag::DiagnosticSeverity::Error;
            d.message = "Invalid directive syntax ':click='; use '@click=' for event listeners or '$bind=' for two-way bindings";
            fillSpan(d.span, badDir, badDir + 7);
            diagnostics.push_back(std::move(d));
            badDir += 7;
        }

        return diagnostics;
    }
};

} // namespace keety::lint
