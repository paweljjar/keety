#include <catch2/catch_test_macros.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/lint/rules/empty_block_rule.hpp>
#include <keety/lint/rules/redundant_type_annotation_rule.hpp>
#include <keety/lint/rules/unnecessary_mut_rule.hpp>
#include <keety/lint/rules/variable_shadowing_rule.hpp>

using namespace keety;
using namespace keety::lint;

TEST_CASE("Category B: L010 unnecessary-mut", "[lint][mutability]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<UnnecessaryMutRule>());

    SECTION("Detects unmutated mut parameter") {
        std::string src =
            "fn compute(mut a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 1);
        CHECK(diags[0].ruleCode == "L010");
        CHECK(diags[0].message.find("'a'") != std::string::npos);
    }

    SECTION("Does not flag mut parameter that is reassigned") {
        std::string src =
            "fn compute(mut a: int): int {\n"
            "    a = a * 2;\n"
            "    return a;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }

    SECTION("Does not flag mut parameter passed as mut argument") {
        std::string src =
            "fn helper(mut x: int): void { x += 1; }\n"
            "fn caller(mut val: int): int {\n"
            "    helper(mut val);\n"
            "    return val;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category B: L011 variable-shadowing", "[lint][scoping]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<VariableShadowingRule>());

    SECTION("Detects local variable shadowing outer parameter or variable") {
        std::string src =
            "let globalVal: int = 100;\n"
            "\n"
            "fn test(param: int): int {\n"
            "    let param: int = 200;\n"
            "    if (param > 0) {\n"
            "        let globalVal: int = 300;\n"
            "        return globalVal;\n"
            "    }\n"
            "    return param;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 2);
        CHECK(diags[0].ruleCode == "L011");
        CHECK(diags[0].message.find("param") != std::string::npos);
        CHECK(diags[1].ruleCode == "L011");
        CHECK(diags[1].message.find("globalVal") != std::string::npos);
    }

    SECTION("Variables in disjoint scopes do not shadow each other") {
        std::string src =
            "fn test(): int {\n"
            "    if (true) {\n"
            "        let x: int = 1;\n"
            "    }\n"
            "    if (true) {\n"
            "        let x: int = 2;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category C: L020 redundant-type-annotation", "[lint][redundancy]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<RedundantTypeAnnotationRule>());

    SECTION("Detects redundant type annotations on primitive literals") {
        std::string src =
            "fn test(): void {\n"
            "    let a: int = 10;\n"
            "    const b: string = \"hello\";\n"
            "    let c: bool = true;\n"
            "    let d: float = 3.14;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 4);
        CHECK(diags[0].ruleCode == "L020");
        CHECK(diags[1].ruleCode == "L020");
        CHECK(diags[2].ruleCode == "L020");
        CHECK(diags[3].ruleCode == "L020");
    }

    SECTION("Does not flag non-literal initializers or complex types") {
        std::string src =
            "fn getVal(): int { return 42; }\n"
            "fn test(): void {\n"
            "    let a: int = getVal();\n"
            "    let b: int | string = 10;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category C: L022 empty-block", "[lint][style]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<EmptyBlockRule>());

    SECTION("Detects empty control flow and function blocks") {
        std::string src =
            "fn emptyFn(): void {}\n"
            "fn test(): void {\n"
            "    if (true) {}\n"
            "    while (false) {}\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 3);
        CHECK(diags[0].ruleCode == "L022");
        CHECK(diags[1].ruleCode == "L022");
        CHECK(diags[2].ruleCode == "L022");
    }

    SECTION("Does not flag non-empty blocks") {
        std::string src =
            "fn normalFn(): void {\n"
            "    if (true) {\n"
            "        let x: int = 1;\n"
            "    }\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}
