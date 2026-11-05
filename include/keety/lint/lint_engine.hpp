#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/lint/lint_config.hpp>
#include <keety/lint/lint_diagnostic.hpp>
#include <keety/lint/lint_rule.hpp>
#include <keety/lint/suppression_map.hpp>
#include <keety/parser/parser.hpp>
#include <keety/sema/scope.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keety::lint {

/// Central engine that coordinates registered lint rules, executes analysis passes,
/// filters out suppressed violations, and applies configuration overrides.
class LintEngine {
public:
    explicit LintEngine(LintConfig config = LintConfig{});

    /// Access the active configuration.
    [[nodiscard]] const LintConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] LintConfig& getConfig() noexcept { return config_; }
    void setConfig(LintConfig config) noexcept { config_ = std::move(config); }

    /// Registers a custom or built-in lint rule.
    void registerRule(std::unique_ptr<LintRule> rule);

    /// Registers all standard default built-in rules (Categories A, B, C, D).
    void registerDefaultRules();

    /// Returns list of all registered rule names.
    [[nodiscard]] std::vector<std::string_view> getRegisteredRuleNames() const;

    /// Runs linting passes on an in-memory source string.
    /// Returns collected, unsuppressed diagnostics.
    [[nodiscard]] std::vector<LintDiagnostic> lintSource(
        std::string_view source,
        std::string_view filePath = "<anonymous>",
        const sema::Scope* globalScope = nullptr
    ) const;

    /// Runs linting passes on an already parsed ModuleAST and comment stream.
    [[nodiscard]] std::vector<LintDiagnostic> lintModule(
        const parser::ModuleAST& moduleAst,
        const std::vector<lexer::Comment>& comments,
        const diag::SourceManager& sourceManager,
        std::string_view sourceCode,
        std::string_view filePath = "<anonymous>",
        const sema::Scope* globalScope = nullptr
    ) const;

    /// Emits collected diagnostics into a DiagnosticEmitter.
    static void emitDiagnostics(
        const std::vector<LintDiagnostic>& diagnostics,
        diag::DiagnosticEmitter& emitter
    );

private:
    LintConfig config_;
    std::vector<std::unique_ptr<LintRule>> rules_;
};

} // namespace keety::lint
