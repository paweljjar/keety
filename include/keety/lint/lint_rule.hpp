#pragma once

#include <keety/ast/base.hpp>
#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/ast/type.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/lint/lint_diagnostic.hpp>
#include <keety/parser/parser.hpp>
#include <keety/sema/scope.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keety::lint {

/// Contextual data and services passed to LintRule passes during inspection.
struct LintContext {
    const parser::ModuleAST& moduleAst;
    const diag::SourceManager* sourceManager{nullptr};
    const sema::Scope* globalScope{nullptr};
    std::string_view sourceCode{};
    std::string_view filePath{};
};

/// Abstract base class for all modular linting passes in keety lint.
class LintRule {
public:
    virtual ~LintRule() = default;

    /// Deterministic uppercase rule code (e.g. "L001", "L010").
    [[nodiscard]] virtual std::string_view getCode() const noexcept = 0;

    /// Canonical hyphenated rule identifier (e.g. "unused-import", "unnecessary-mut").
    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

    /// Human-readable description of what this rule checks.
    [[nodiscard]] virtual std::string_view getDescription() const noexcept = 0;

    /// Default diagnostic severity if unconfigured.
    [[nodiscard]] virtual diag::DiagnosticSeverity getDefaultSeverity() const noexcept {
        return diag::DiagnosticSeverity::Warning;
    }

    /// Primary execution hook: runs analysis on the parsed module AST.
    [[nodiscard]] virtual std::vector<LintDiagnostic> run(const LintContext& context) = 0;
};

} // namespace keety::lint
