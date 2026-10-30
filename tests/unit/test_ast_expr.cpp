#include <catch2/catch_test_macros.hpp>
#include <keety/ast/expr.hpp>

using namespace keety::ast;
using namespace keety::diag;
using namespace keety::lexer;

TEST_CASE("LiteralExpr and IdentExpr creation", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 5, 1, 6}};

    LiteralExpr litInt(span, TokenKind::IntLiteral, "42");
    REQUIRE(litInt.getLiteralKind() == TokenKind::IntLiteral);
    REQUIRE(litInt.getRawText() == "42");

    IdentExpr id(span, "myVar");
    REQUIRE(id.getName() == "myVar");
}

TEST_CASE("BinaryExpr, UnaryExpr, and TernaryExpr composition", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 10, 1, 11}};

    auto left = std::make_unique<IdentExpr>(span, "a");
    auto right = std::make_unique<IdentExpr>(span, "b");
    BinaryExpr bin(span, TokenKind::Plus, std::move(left), std::move(right));
    REQUIRE(bin.getOp() == TokenKind::Plus);
    REQUIRE(bin.getLeft() != nullptr);
    REQUIRE(bin.getRight() != nullptr);

    auto operand = std::make_unique<IdentExpr>(span, "x");
    UnaryExpr un(span, TokenKind::Exclaim, std::move(operand), true);
    REQUIRE(un.getOp() == TokenKind::Exclaim);
    REQUIRE(un.isPrefix());

    auto cond = std::make_unique<IdentExpr>(span, "isValid");
    auto thenE = std::make_unique<LiteralExpr>(span, TokenKind::IntLiteral, "1");
    auto elseE = std::make_unique<LiteralExpr>(span, TokenKind::IntLiteral, "0");
    TernaryExpr ternary(span, std::move(cond), std::move(thenE), std::move(elseE));
    REQUIRE(ternary.getCondition() != nullptr);
    REQUIRE(ternary.getThenExpr() != nullptr);
    REQUIRE(ternary.getElseExpr() != nullptr);
}

TEST_CASE("CallExpr with mutable borrowing and generic type arguments", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 20, 1, 21}};

    auto callee = std::make_unique<IdentExpr>(span, "swap");
    std::vector<CallArg> args;
    args.push_back(CallArg{span, std::make_unique<IdentExpr>(span, "u"), true});  // mut u
    args.push_back(CallArg{span, std::make_unique<IdentExpr>(span, "v"), true});  // mut v

    CallExpr call(span, std::move(callee), std::move(args));
    REQUIRE(call.getArgs().size() == 2);
    REQUIRE(call.getArgs()[0].isMut);
    REQUIRE(call.getArgs()[1].isMut);
}

TEST_CASE("ZonkExpr fallback vs recovery block forms", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 15, 1, 16}};

    // 1. expr zonk -1
    auto prim1 = std::make_unique<IdentExpr>(span, "val");
    auto fb1 = std::make_unique<LiteralExpr>(span, TokenKind::IntLiteral, "-1");
    ZonkExpr zonkFallback(span, std::move(prim1), std::move(fb1));
    REQUIRE_FALSE(zonkFallback.hasRecoveryBlock());
    REQUIRE(zonkFallback.getFallback() != nullptr);

    // 2. expr zonk (err) { yield 404; }
    auto prim2 = std::make_unique<IdentExpr>(span, "compute");
    auto block = std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{});
    ZonkExpr zonkBlock(span, std::move(prim2), nullptr, "err", std::move(block));
    REQUIRE(zonkBlock.hasRecoveryBlock());
    REQUIRE(zonkBlock.getErrorVarName() == "err");
}

TEST_CASE("Collections, Casts, and AwaitExpr", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 15, 1, 16}};

    // Array literal: [1, 2]
    std::vector<std::unique_ptr<Expr>> elems;
    elems.push_back(std::make_unique<LiteralExpr>(span, TokenKind::IntLiteral, "1"));
    elems.push_back(std::make_unique<LiteralExpr>(span, TokenKind::IntLiteral, "2"));
    ArrayLitExpr arr(span, std::move(elems));
    REQUIRE(arr.getElements().size() == 2);

    // TypeofExpr
    TypeofExpr typeofNode(span, std::make_unique<IdentExpr>(span, "x"));
    REQUIRE(typeofNode.getExpr() != nullptr);

    // AwaitExpr
    AwaitExpr awaitNode(span, std::make_unique<IdentExpr>(span, "fetchUser"));
    REQUIRE(awaitNode.getTarget() != nullptr);
}
