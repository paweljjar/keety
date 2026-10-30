#include <catch2/catch_test_macros.hpp>
#include <keety/parser/parser.hpp>

using namespace keety::parser;
using namespace keety::lexer;
using namespace keety::ast;

TEST_CASE("Pratt parser respects arithmetic operator precedence (2 + 3 * 4)", "[parser]") {
    // 2 + 3 * 4 should parse as: 2 + (3 * 4)
    std::string_view code = "2 + 3 * 4;";
    Lexer lexer(1, code);
    Parser parser(lexer);

    auto stmt = parser.parseStatement();
    auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
    REQUIRE(exprStmt != nullptr);

    auto* plusExpr = dynamic_cast<const BinaryExpr*>(exprStmt->getExpression());
    REQUIRE(plusExpr != nullptr);
    REQUIRE(plusExpr->getOp() == TokenKind::Plus);

    // Left is 2
    auto* leftLit = dynamic_cast<const LiteralExpr*>(plusExpr->getLeft());
    REQUIRE(leftLit != nullptr);
    REQUIRE(leftLit->getRawText() == "2");

    // Right is 3 * 4
    auto* rightMul = dynamic_cast<const BinaryExpr*>(plusExpr->getRight());
    REQUIRE(rightMul != nullptr);
    REQUIRE(rightMul->getOp() == TokenKind::Star);
}

TEST_CASE("Pratt parser respects parenthesized grouping ( (2 + 3) * 4 )", "[parser]") {
    std::string_view code = "(2 + 3) * 4;";
    Lexer lexer(1, code);
    Parser parser(lexer);

    auto stmt = parser.parseStatement();
    auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
    REQUIRE(exprStmt != nullptr);

    auto* mulExpr = dynamic_cast<const BinaryExpr*>(exprStmt->getExpression());
    REQUIRE(mulExpr != nullptr);
    REQUIRE(mulExpr->getOp() == TokenKind::Star);

    // Left is (2 + 3)
    auto* leftPlus = dynamic_cast<const BinaryExpr*>(mulExpr->getLeft());
    REQUIRE(leftPlus != nullptr);
    REQUIRE(leftPlus->getOp() == TokenKind::Plus);
}

TEST_CASE("Pratt parser handles ternary conditional operator (cond ? a : b)", "[parser]") {
    std::string_view code = "x > 0 ? 100 : -100;";
    Lexer lexer(1, code);
    Parser parser(lexer);

    auto stmt = parser.parseStatement();
    auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
    REQUIRE(exprStmt != nullptr);

    auto* ternary = dynamic_cast<const TernaryExpr*>(exprStmt->getExpression());
    REQUIRE(ternary != nullptr);
    REQUIRE(dynamic_cast<const BinaryExpr*>(ternary->getCondition()) != nullptr);
    REQUIRE(dynamic_cast<const LiteralExpr*>(ternary->getThenExpr()) != nullptr);
    REQUIRE(dynamic_cast<const UnaryExpr*>(ternary->getElseExpr()) != nullptr);
}

TEST_CASE("Pratt parser handles member access and calls with mut borrowing", "[parser]") {
    // router.swapBuffers(mut poolA, mut poolB);
    std::string_view code = "router.swapBuffers(mut poolA, mut poolB);";
    Lexer lexer(1, code);
    Parser parser(lexer);

    auto stmt = parser.parseStatement();
    auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
    REQUIRE(exprStmt != nullptr);

    auto* call = dynamic_cast<const CallExpr*>(exprStmt->getExpression());
    REQUIRE(call != nullptr);
    REQUIRE(call->getArgs().size() == 2);
    REQUIRE(call->getArgs()[0].isMut);
    REQUIRE(call->getArgs()[1].isMut);

    auto* member = dynamic_cast<const MemberExpr*>(call->getCallee());
    REQUIRE(member != nullptr);
    REQUIRE(member->getMemberName() == "swapBuffers");
}
