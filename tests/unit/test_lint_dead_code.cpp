#include <catch2/catch_test_macros.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/lint/rules/unreachable_code_rule.hpp>
#include <keety/lint/rules/unused_import_rule.hpp>
#include <keety/lint/rules/unused_parameter_rule.hpp>
#include <keety/lint/rules/unused_variable_rule.hpp>

using namespace keety;
using namespace keety::lint;

TEST_CASE("Category A: L001 unused-import", "[lint][dead-code][import]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<UnusedImportRule>());

    SECTION("Detects unused module import and selective import") {
        std::string src =
            "import io;\n"
            "import math;\n"
            "from \"./helpers\" import usedHelper, unusedHelper;\n"
            "\n"
            "fn main(): void {\n"
            "    io.println(\"Hello\");\n"
            "    usedHelper();\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 2);

        CHECK(diags[0].ruleCode == "L001");
        CHECK(diags[0].message.find("math") != std::string::npos);

        CHECK(diags[1].ruleCode == "L001");
        CHECK(diags[1].message.find("unusedHelper") != std::string::npos);
    }

    SECTION("Does not flag used imports or re-exports") {
        std::string src =
            "import io as console;\n"
            "from \"./math\" import add;\n"
            "\n"
            "fn calculate(): int {\n"
            "    console.println(\"calc\");\n"
            "    return add(1, 2);\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }

    SECTION("Suppression comment disables unused-import warning") {
        std::string src =
            "// keetlint:disable-next-line unused-import\n"
            "import math;\n"
            "fn main(): void {}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category A: L002 unused-variable", "[lint][dead-code][variable]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<UnusedVariableRule>());

    SECTION("Detects unused local variables") {
        std::string src =
            "fn compute(): int {\n"
            "    let used: int = 10;\n"
            "    let unused: int = 20;\n"
            "    const UNUSED_CONST: int = 30;\n"
            "    return used;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 2);
        CHECK(diags[0].ruleCode == "L002");
        CHECK(diags[0].message.find("unused") != std::string::npos);
        CHECK(diags[1].ruleCode == "L002");
        CHECK(diags[1].message.find("UNUSED_CONST") != std::string::npos);
    }

    SECTION("Variables prefixed with '_' are ignored") {
        std::string src =
            "fn compute(): int {\n"
            "    let _dummy: int = 42;\n"
            "    let x: int = 100;\n"
            "    return x;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }

    SECTION("Variables read in nested blocks are not flagged") {
        std::string src =
            "fn search(target: int): bool {\n"
            "    let found: bool = false;\n"
            "    if (target > 0) {\n"
            "        found = true;\n"
            "    }\n"
            "    return found;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category A: L003 unused-parameter", "[lint][dead-code][parameter]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<UnusedParameterRule>());

    SECTION("Detects unused parameters in functions and closures") {
        std::string src =
            "fn process(a: int, b: int): int {\n"
            "    let f = fn(unusedInClosure: int, x: int): int {\n"
            "        return x * 2;\n"
            "    };\n"
            "    return a + f(1, 2);\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 2);

        CHECK(diags[0].ruleCode == "L003");
        CHECK(diags[0].message.find("'b'") != std::string::npos);

        CHECK(diags[1].ruleCode == "L003");
        CHECK(diags[1].message.find("'unusedInClosure'") != std::string::npos);
    }

    SECTION("Parameters prefixed with '_' are ignored") {
        std::string src =
            "fn handleCallback(_event: string, value: int): int {\n"
            "    return value + 1;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}

TEST_CASE("Category A: L004 unreachable-code", "[lint][dead-code][unreachable]") {
    LintEngine engine;
    engine.registerRule(std::make_unique<UnreachableCodeRule>());

    SECTION("Detects code after return, break, continue, yield, zonk") {
        std::string src =
            "fn testUnreachable(): int {\n"
            "    let x: int = 1;\n"
            "    return x;\n"
            "    let y: int = 2;\n"
            "    x = y;\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 2);
        CHECK(diags[0].ruleCode == "L004");
        CHECK(diags[1].ruleCode == "L004");
    }

    SECTION("Detects unreachable statements inside loop after break") {
        std::string src =
            "fn loopTest(): void {\n"
            "    while (true) {\n"
            "        break;\n"
            "        let dead: int = 10;\n"
            "    }\n"
            "}\n";

        auto diags = engine.lintSource(src);
        REQUIRE(diags.size() == 1);
        CHECK(diags[0].ruleCode == "L004");
    }

    SECTION("Reachable branches in conditional are not flagged") {
        std::string src =
            "fn branchTest(flag: bool): int {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    } else {\n"
            "        return 2;\n"
            "    }\n"
            "}\n";

        auto diags = engine.lintSource(src);
        CHECK(diags.empty());
    }
}
