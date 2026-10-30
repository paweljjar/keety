#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/parser/parser.hpp>

using namespace keety::parser;
using namespace keety::lexer;
using namespace keety::ast;
using namespace keety::diag;

TEST_CASE("Parser parses while, classic for, and switch statements with labels", "[parser]") {
    SECTION("While loop with label and labeled break") {
        std::string_view code = "while (attempts < 3) as retryLoop { break retryLoop; }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* whileNode = dynamic_cast<WhileStmt*>(stmt.get());
        REQUIRE(whileNode != nullptr);
        REQUIRE(whileNode->hasLabel());
        REQUIRE(whileNode->getLabel() == "retryLoop");

        const auto* body = whileNode->getBody();
        REQUIRE(body != nullptr);
        REQUIRE(body->getStatements().size() == 1);

        auto* brk = dynamic_cast<const BreakStmt*>(body->getStatements()[0].get());
        REQUIRE(brk != nullptr);
        REQUIRE(brk->hasTargetLabel());
        REQUIRE(brk->getTargetLabel() == "retryLoop");
    }

    SECTION("Classic for loop with label and labeled continue") {
        std::string_view code = "for (let i: int = 0; i < 10; i++) as forLoop { continue forLoop; }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forNode = dynamic_cast<ForStmt*>(stmt.get());
        REQUIRE(forNode != nullptr);
        REQUIRE(forNode->hasLabel());
        REQUIRE(forNode->getLabel() == "forLoop");
        REQUIRE(forNode->getInit() != nullptr);
        REQUIRE(forNode->getCondition() != nullptr);
        REQUIRE(forNode->getPost() != nullptr);

        const auto* body = forNode->getBody();
        REQUIRE(body != nullptr);
        REQUIRE(body->getStatements().size() == 1);

        auto* cont = dynamic_cast<const ContinueStmt*>(body->getStatements()[0].get());
        REQUIRE(cont != nullptr);
        REQUIRE(cont->hasTargetLabel());
        REQUIRE(cont->getTargetLabel() == "forLoop");
    }

    SECTION("Classic for loop with omitted condition and post: for (;;)") {
        std::string_view code = "for (;;) as infLoop { break infLoop; }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forNode = dynamic_cast<ForStmt*>(stmt.get());
        REQUIRE(forNode != nullptr);
        REQUIRE(forNode->hasLabel());
        REQUIRE(forNode->getLabel() == "infLoop");
        REQUIRE(forNode->getInit() == nullptr);
        REQUIRE(forNode->getCondition() == nullptr);
        REQUIRE(forNode->getPost() == nullptr);
    }

    SECTION("Switch statement with label and labeled break") {
        std::string_view code = "switch (statusCode) as handleStatus { case (200, 204) { break handleStatus; } }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* switchNode = dynamic_cast<SwitchStmt*>(stmt.get());
        REQUIRE(switchNode != nullptr);
        REQUIRE(switchNode->hasLabel());
        REQUIRE(switchNode->getLabel() == "handleStatus");
        REQUIRE(switchNode->getCases().size() == 1);

        const auto& c = switchNode->getCases()[0];
        REQUIRE(c.values.size() == 2);
        REQUIRE(c.body->getStatements().size() == 1);

        auto* brk = dynamic_cast<const BreakStmt*>(c.body->getStatements()[0].get());
        REQUIRE(brk != nullptr);
        REQUIRE(brk->hasTargetLabel());
        REQUIRE(brk->getTargetLabel() == "handleStatus");
    }
}

TEST_CASE("Parser parses for-in collection loops with and without labels", "[parser]") {
    SECTION("Array value-only iteration: for (const name: string in names)") {
        std::string_view code = "for (const name: string in names) as namesLoop { io.println(name); }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forIn = dynamic_cast<ForInStmt*>(stmt.get());
        REQUIRE(forIn != nullptr);
        REQUIRE(forIn->isConst());
        REQUIRE_FALSE(forIn->isKeyValue());
        REQUIRE(forIn->getKeyName().empty());
        REQUIRE(forIn->getValueName() == "name");
        REQUIRE(forIn->getValueType() != nullptr);
        REQUIRE(forIn->hasLabel());
        REQUIRE(forIn->getLabel() == "namesLoop");
        REQUIRE(forIn->getCollection() != nullptr);
    }

    SECTION("Array index + value iteration: for (let idx: int, name: string in names)") {
        std::string_view code = "for (let idx: int, name: string in names) { }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forIn = dynamic_cast<ForInStmt*>(stmt.get());
        REQUIRE(forIn != nullptr);
        REQUIRE_FALSE(forIn->isConst());
        REQUIRE(forIn->isKeyValue());
        REQUIRE(forIn->getKeyName() == "idx");
        REQUIRE(forIn->getValueName() == "name");
        REQUIRE(forIn->getKeyType() != nullptr);
        REQUIRE(forIn->getValueType() != nullptr);
        REQUIRE_FALSE(forIn->hasLabel());
    }

    SECTION("Map key + value iteration: for (const proto: string, port: int in ports)") {
        std::string_view code = "for (const proto: string, port: int in ports) as mapLoop { }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forIn = dynamic_cast<ForInStmt*>(stmt.get());
        REQUIRE(forIn != nullptr);
        REQUIRE(forIn->isConst());
        REQUIRE(forIn->isKeyValue());
        REQUIRE(forIn->getKeyName() == "proto");
        REQUIRE(forIn->getValueName() == "port");
        REQUIRE(forIn->hasLabel());
        REQUIRE(forIn->getLabel() == "mapLoop");
    }

    SECTION("Reusing pre-declared variables: for (item in names)") {
        std::string_view code = "for (item in names) { }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forIn = dynamic_cast<ForInStmt*>(stmt.get());
        REQUIRE(forIn != nullptr);
        REQUIRE_FALSE(forIn->isKeyValue());
        REQUIRE(forIn->getValueName() == "item");
        REQUIRE(forIn->getValueType() == nullptr);
    }

    SECTION("Blank identifier in for-in binding: for (let _: string, val: int in scores)") {
        std::string_view code = "for (let _: string, val: int in scores) { }";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* forIn = dynamic_cast<ForInStmt*>(stmt.get());
        REQUIRE(forIn != nullptr);
        REQUIRE(forIn->isKeyValue());
        REQUIRE(forIn->getKeyName() == "_");
        REQUIRE(forIn->getValueName() == "val");
    }
}

TEST_CASE("Parser enforces mandatory statement-terminating semicolons", "[parser]") {
    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);

    SECTION("Expression statement missing semicolon produces ExpectedSemicolon error") {
        std::string code = "let a: int = 1; x = 42\nreturn;";
        FileId fileId = sm.addBuffer("test.keet", code);
        Lexer lexer(fileId, code);
        Parser parser(lexer, &emitter);

        auto mod = parser.parseModule("test");
        REQUIRE(emitter.hasErrors());
        REQUIRE(emitter.getErrorCount() > 0);
    }

    SECTION("Break statement missing semicolon produces ExpectedSemicolon error") {
        std::string code = "break myLabel\nreturn;";
        FileId fileId = sm.addBuffer("test.keet", code);
        Lexer lexer(fileId, code);
        Parser parser(lexer, &emitter);

        auto stmt = parser.parseStatement();
        REQUIRE(emitter.hasErrors());
    }

    SECTION("Empty statements (;) are accepted gracefully without error") {
        std::string code = ";; let x: int = 10; ;;";
        FileId fileId = sm.addBuffer("test.keet", code);
        Lexer lexer(fileId, code);
        Parser parser(lexer, &emitter);

        auto stmt = parser.parseStatement();
        REQUIRE_FALSE(emitter.hasErrors());
    }
}
