#include <keety/lint/lint_engine.hpp>
#include <keety/lint/rules/empty_block_rule.hpp>
#include <keety/lint/rules/redundant_type_annotation_rule.hpp>
#include <keety/lint/rules/unnecessary_mut_rule.hpp>
#include <keety/lint/rules/unreachable_code_rule.hpp>
#include <keety/lint/rules/unused_import_rule.hpp>
#include <keety/lint/rules/unused_parameter_rule.hpp>
#include <keety/lint/rules/unused_variable_rule.hpp>
#include <keety/lint/rules/variable_shadowing_rule.hpp>

#include <algorithm>

namespace keety::lint {

LintEngine::LintEngine(LintConfig config) : config_(std::move(config)) {}

void LintEngine::registerRule(std::unique_ptr<LintRule> rule) {
    if (rule) {
        rules_.push_back(std::move(rule));
    }
}

void LintEngine::registerDefaultRules() {
    // Category A: Dead Code & Hygiene
    registerRule(std::make_unique<UnusedImportRule>());
    registerRule(std::make_unique<UnusedVariableRule>());
    registerRule(std::make_unique<UnusedParameterRule>());
    registerRule(std::make_unique<UnreachableCodeRule>());

    // Category B: Immutability & Scoping
    registerRule(std::make_unique<UnnecessaryMutRule>());
    registerRule(std::make_unique<VariableShadowingRule>());

    // Category C: Redundancy & Idiomatic Style
    // Note: RedundantTypeAnnotationRule is excluded from default rules because KeetScript
    // strictly mandates explicit type annotations on all variable and constant declarations.
    registerRule(std::make_unique<EmptyBlockRule>());
}

std::vector<std::string_view> LintEngine::getRegisteredRuleNames() const {
    std::vector<std::string_view> names;
    names.reserve(rules_.size());
    for (const auto& r : rules_) {
        names.push_back(r->getName());
    }
    return names;
}

std::vector<LintDiagnostic> LintEngine::lintSource(
    std::string_view source,
    std::string_view filePath,
    const sema::Scope* globalScope
) const {
    diag::SourceManager sm;
    auto fileId = sm.addBuffer(std::string(filePath), std::string(source));

    // Lex with comment capture enabled
    lexer::Lexer lexer(fileId, source);
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer);
    auto moduleAst = parser.parseModule(std::string(filePath));

    auto comments = lexer.getCapturedComments();
    return lintModule(moduleAst, comments, sm, source, filePath, globalScope);
}

std::vector<LintDiagnostic> LintEngine::lintModule(
    const parser::ModuleAST& moduleAst,
    const std::vector<lexer::Comment>& comments,
    const diag::SourceManager& sourceManager,
    std::string_view sourceCode,
    std::string_view filePath,
    const sema::Scope* globalScope
) const {
    std::vector<LintDiagnostic> activeDiagnostics;

    // Check if the file is ignored in config
    if (config_.isPathIgnored(std::filesystem::path(filePath))) {
        return activeDiagnostics;
    }

    // Build suppression map from comments
    SuppressionMap suppressionMap = SuppressionMap::fromComments(comments);

    // Prepare context
    LintContext ctx{
        moduleAst,
        &sourceManager,
        globalScope,
        sourceCode,
        filePath
    };

    // Execute each registered rule
    for (const auto& rule : rules_) {
        auto effectiveSev = config_.getRuleSeverity(
            rule->getName(),
            rule->getCode(),
            rule->getDefaultSeverity()
        );

        // If rule is disabled ("off"), skip execution
        if (!effectiveSev.has_value()) {
            continue;
        }

        auto diags = rule->run(ctx);

        for (auto& d : diags) {
            // Apply configured severity override
            d.severity = effectiveSev.value();

            // Filter out if suppressed at source line
            size_t line = d.span.start.line;
            if (suppressionMap.isSuppressed(line, d.ruleName, d.ruleCode)) {
                continue;
            }

            activeDiagnostics.push_back(std::move(d));
        }
    }

    // Stable sort diagnostics by source location
    std::stable_sort(activeDiagnostics.begin(), activeDiagnostics.end(),
        [](const LintDiagnostic& a, const LintDiagnostic& b) {
            if (a.span.start.line != b.span.start.line) {
                return a.span.start.line < b.span.start.line;
            }
            return a.span.start.column < b.span.start.column;
        }
    );

    return activeDiagnostics;
}

void LintEngine::emitDiagnostics(
    const std::vector<LintDiagnostic>& diagnostics,
    diag::DiagnosticEmitter& emitter
) {
    for (const auto& d : diagnostics) {
        emitter.report(d.toCompilerDiagnostic());
    }
}

} // namespace keety::lint
