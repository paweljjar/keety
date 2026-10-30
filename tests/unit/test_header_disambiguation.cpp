#include <catch2/catch_test_macros.hpp>
#include <keety/parser/parser.hpp>

using namespace keety::parser;
using namespace keety::lexer;
using namespace keety::ast;

TEST_CASE("Parser disambiguates IfHeaderInit from simple if condition", "[parser]") {
    // 1. Simple if: if (x > 0) { ... }
    std::string_view code1 = "if (x > 0) { return 1; }";
    Lexer lexer1(1, code1);
    Parser parser1(lexer1);

    auto stmt1 = parser1.parseStatement();
    auto* ifNode1 = dynamic_cast<IfStmt*>(stmt1.get());
    REQUIRE(ifNode1 != nullptr);
    REQUIRE_FALSE(ifNode1->hasInit());
    REQUIRE(ifNode1->getCondition() != nullptr);

    // 2. If with header initialization: if (let res: int = calculate(); res > 0) { ... }
    std::string_view code2 = "if (let res: int = 42; res > 0) { return res; }";
    Lexer lexer2(1, code2);
    Parser parser2(lexer2);

    auto stmt2 = parser2.parseStatement();
    auto* ifNode2 = dynamic_cast<IfStmt*>(stmt2.get());
    REQUIRE(ifNode2 != nullptr);
    REQUIRE(ifNode2->hasInit());
    auto* initDecl = dynamic_cast<const DeclStmt*>(ifNode2->getInit());
    REQUIRE(initDecl != nullptr);
    auto* varDecl = dynamic_cast<const VarDecl*>(initDecl->getDeclaration());
    REQUIRE(varDecl != nullptr);
    REQUIRE(varDecl->getName() == "res");
    REQUIRE(ifNode2->getCondition() != nullptr);
}

TEST_CASE("Parser disambiguates WhileHeaderInit and SwitchHeaderInit", "[parser]") {
    // 1. While with init: while (let i: int = 0; i < 10) as myLoop { ... }
    std::string_view codeWhile = "while (let i: int = 0; i < 10) as myLoop { i++; }";
    Lexer lexerW(1, codeWhile);
    Parser parserW(lexerW);

    auto stmtW = parserW.parseStatement();
    auto* whileNode = dynamic_cast<WhileStmt*>(stmtW.get());
    REQUIRE(whileNode != nullptr);
    REQUIRE(whileNode->hasInit());
    REQUIRE(whileNode->hasLabel());
    REQUIRE(whileNode->getLabel() == "myLoop");

    // 2. Switch with init: switch (let status: int = getStatus(); status) { case (200) { return true; } }
    std::string_view codeSwitch = "switch (let status: int = 200; status) { case (200) { return true; } }";
    Lexer lexerS(1, codeSwitch);
    Parser parserS(lexerS);

    auto stmtS = parserS.parseStatement();
    auto* switchNode = dynamic_cast<SwitchStmt*>(stmtS.get());
    REQUIRE(switchNode != nullptr);
    REQUIRE(switchNode->hasInit());
    REQUIRE(switchNode->getDiscriminant() != nullptr);
    REQUIRE(switchNode->getCases().size() == 1);
}
