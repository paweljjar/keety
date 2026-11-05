#include <catch2/catch_test_macros.hpp>
#include <keety/lint/lint_config.hpp>
#include <keety/lint/lint_diagnostic.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/lint/lint_rule.hpp>
#include <keety/lint/suppression_map.hpp>

using namespace keety;
using namespace keety::lint;

namespace {

/// A mock lint rule for testing registration, execution, and suppression.
class MockAlwaysWarnRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L999"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "mock-warning"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override { return "Mock rule that emits a warning on line 2"; }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Warning;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext&) override {
        std::vector<LintDiagnostic> diags;
        // Emit a diagnostic on line 2
        diag::SourceLocation start{1, 0, 2, 1};
        diag::SourceLocation end{1, 10, 2, 10};
        diag::SourceSpan span{start, end};
        diags.emplace_back(
            std::string(getCode()),
            std::string(getName()),
            getDefaultSeverity(),
            "Mock warning detected",
            span,
            FixEdit{span, "/* fixed */"}
        );
        return diags;
    }
};

class MockErrorRule : public LintRule {
public:
    [[nodiscard]] std::string_view getCode() const noexcept override { return "L998"; }
    [[nodiscard]] std::string_view getName() const noexcept override { return "mock-error"; }
    [[nodiscard]] std::string_view getDescription() const noexcept override { return "Mock rule that emits an error on line 4"; }
    [[nodiscard]] diag::DiagnosticSeverity getDefaultSeverity() const noexcept override {
        return diag::DiagnosticSeverity::Error;
    }

    [[nodiscard]] std::vector<LintDiagnostic> run(const LintContext&) override {
        std::vector<LintDiagnostic> diags;
        diag::SourceLocation start{1, 0, 4, 1};
        diag::SourceLocation end{1, 10, 4, 10};
        diag::SourceSpan span{start, end};
        diags.emplace_back(
            std::string(getCode()),
            std::string(getName()),
            getDefaultSeverity(),
            "Mock error detected",
            span
        );
        return diags;
    }
};

} // namespace

TEST_CASE("LintConfig loads and resolves severities and overrides", "[lint][config]") {
    SECTION("Default severities without config overrides") {
        LintConfig config;
        CHECK(config.getRuleSeverity("unused-import", "L001", diag::DiagnosticSeverity::Warning) == diag::DiagnosticSeverity::Warning);
        CHECK(config.getRuleSeverity("unreachable-code", "L004", diag::DiagnosticSeverity::Error) == diag::DiagnosticSeverity::Error);
    }

    SECTION("Strict mode elevates warnings to errors") {
        LintConfig config;
        config.setStrict(true);
        CHECK(config.getRuleSeverity("unused-import", "L001", diag::DiagnosticSeverity::Warning) == diag::DiagnosticSeverity::Error);
    }

    SECTION("Rule overrides by name and code") {
        LintConfig config;
        config.setRuleSeverity("unused-import", diag::DiagnosticSeverity::Error);
        config.setRuleSeverity("L002", std::nullopt); // disabled

        CHECK(config.getRuleSeverity("unused-import", "L001", diag::DiagnosticSeverity::Warning) == diag::DiagnosticSeverity::Error);
        CHECK(config.getRuleSeverity("unused-variable", "L002", diag::DiagnosticSeverity::Warning) == std::nullopt);
    }

    SECTION("Parse JSON configuration") {
        std::string jsonStr = R"({
            "strict": false,
            "rules": {
                "unused-import": "error",
                "unused-variable": "off",
                "L010": "info"
            },
            "ignore": [
                "vendor/**",
                "generated/*.keet"
            ]
        })";

        std::string err;
        auto config = LintConfig::parseJson(jsonStr, &err);
        CHECK(err.empty());
        CHECK_FALSE(config.isStrict());
        CHECK(config.getRuleSeverity("unused-import", "L001", diag::DiagnosticSeverity::Warning) == diag::DiagnosticSeverity::Error);
        CHECK(config.getRuleSeverity("unused-variable", "L002", diag::DiagnosticSeverity::Warning) == std::nullopt);
        CHECK(config.getRuleSeverity("unnecessary-mut", "L010", diag::DiagnosticSeverity::Warning) == diag::DiagnosticSeverity::Note);

        CHECK(config.isPathIgnored("vendor/lib/test.keet"));
        CHECK(config.isPathIgnored("generated/*.keet"));
        CHECK_FALSE(config.isPathIgnored("src/main.keet"));
    }
}

TEST_CASE("SuppressionMap correctly handles line and range directives", "[lint][suppression]") {
    SECTION("disable-next-line comment suppresses target rule on next line") {
        std::string src =
            "// keetlint:disable-next-line mock-warning\n"
            "let x: int = 1;\n"
            "let y: int = 2;\n";

        auto map = SuppressionMap::fromSource(src);
        CHECK(map.isSuppressed(2, "mock-warning", "L999"));
        CHECK_FALSE(map.isSuppressed(2, "other-rule", "L001"));
        CHECK_FALSE(map.isSuppressed(3, "mock-warning", "L999"));
    }

    SECTION("disable and enable block comments") {
        std::string src =
            "let a = 1;\n"
            "// keetlint:disable mock-warning\n"
            "let b = 2;\n"
            "let c = 3;\n"
            "// keetlint:enable mock-warning\n"
            "let d = 4;\n";

        auto map = SuppressionMap::fromSource(src);
        CHECK_FALSE(map.isSuppressed(1, "mock-warning", "L999"));
        CHECK(map.isSuppressed(2, "mock-warning", "L999"));
        CHECK(map.isSuppressed(3, "mock-warning", "L999"));
        CHECK(map.isSuppressed(4, "mock-warning", "L999"));
        CHECK_FALSE(map.isSuppressed(6, "mock-warning", "L999"));
    }

    SECTION("disable-next-line with wildcard suppresses all rules") {
        std::string src =
            "// keetlint:disable-next-line\n"
            "let a = 1;\n";

        auto map = SuppressionMap::fromSource(src);
        CHECK(map.isSuppressed(2, "any-rule", "L001"));
        CHECK(map.isSuppressed(2, "mock-warning", "L999"));
    }
}

TEST_CASE("LintEngine coordinates rule registration and execution", "[lint][engine]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<MockAlwaysWarnRule>());
    engine.registerRule(std::make_unique<MockErrorRule>());

    auto ruleNames = engine.getRegisteredRuleNames();
    CHECK(ruleNames.size() == 2);
    CHECK(ruleNames[0] == "mock-warning");
    CHECK(ruleNames[1] == "mock-error");

    SECTION("Running rules emits diagnostics") {
        std::string src =
            "let a = 1;\n"
            "let b = 2;\n"
            "let c = 3;\n"
            "let d = 4;\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.size() == 2);
        CHECK(diags[0].ruleCode == "L999");
        CHECK(diags[0].severity == diag::DiagnosticSeverity::Warning);
        CHECK(diags[1].ruleCode == "L998");
        CHECK(diags[1].severity == diag::DiagnosticSeverity::Error);
    }

    SECTION("Suppressed lines are filtered out") {
        std::string src =
            "// keetlint:disable-next-line mock-warning\n"
            "let b = 2;\n"
            "let c = 3;\n"
            "let d = 4;\n";

        auto diags = engine.lintSource(src);
        // Only line 4 error should remain
        CHECK(diags.size() == 1);
        CHECK(diags[0].ruleCode == "L998");
    }

    SECTION("Disabled rule in config is skipped") {
        LintConfig config;
        config.setRuleSeverity("mock-warning", std::nullopt);
        engine.setConfig(config);

        std::string src =
            "let a = 1;\n"
            "let b = 2;\n"
            "let c = 3;\n"
            "let d = 4;\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.size() == 1);
        CHECK(diags[0].ruleCode == "L998");
    }
}
