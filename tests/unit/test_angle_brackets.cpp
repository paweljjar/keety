#include <catch2/catch_test_macros.hpp>
#include <keety/lexer/lexer.hpp>

using namespace keety::lexer;

TEST_CASE("Lexer angle brackets in expression vs type contexts", "[lexer]") {
    // In normal expressions:
    // a < b; a <= b; a > b; a >= b;
    std::string_view exprCode = "a < b; a <= b; a > b; a >= b;";
    Lexer lexerExpr(1, exprCode);

    auto tokensExpr = lexerExpr.tokenizeAll();
    // 0: a, 1: <, 2: b, 3: ;
    // 4: a, 5: <=, 6: b, 7: ;
    // 8: a, 9: >, 10: b, 11: ;
    // 12: a, 13: >=, 14: b, 15: ;
    // 16: EOF
    REQUIRE(tokensExpr[1].is(TokenKind::Less));
    REQUIRE(tokensExpr[5].is(TokenKind::LessEqual));
    REQUIRE(tokensExpr[9].is(TokenKind::Greater));
    REQUIRE(tokensExpr[13].is(TokenKind::GreaterEqual));
}

TEST_CASE("Lexer nested generic types: array<array<int>> without shift >>", "[lexer]") {
    // Nested generic type annotation: array<array<int>> = [];
    std::string_view typeCode = "let matrix: array<array<int>> = [];";
    Lexer lexer(1, typeCode);

    // Emulate parser encountering ':' and entering generic type annotation context
    // 0: let, 1: matrix, 2: :
    REQUIRE(lexer.nextToken().is(TokenKind::KwLet));
    REQUIRE(lexer.nextToken().is(TokenKind::Identifier));
    REQUIRE(lexer.nextToken().is(TokenKind::Colon));

    // After colon, we enter a type context
    lexer.setMode(LexerMode::TypeContext);

    REQUIRE(lexer.nextToken().is(TokenKind::KwArray)); // array
    REQUIRE(lexer.nextToken().is(TokenKind::Less));    // < (depth 1)
    REQUIRE(lexer.nextToken().is(TokenKind::KwArray)); // array
    REQUIRE(lexer.nextToken().is(TokenKind::Less));    // < (depth 2)
    REQUIRE(lexer.nextToken().is(TokenKind::KwInt));   // int

    // First '>' closes innermost array<int>
    Token gt1 = lexer.nextToken();
    REQUIRE(gt1.is(TokenKind::Greater));
    REQUIRE(gt1.lexeme == ">");

    // Second '>' closes outer array<array<int>>
    Token gt2 = lexer.nextToken();
    REQUIRE(gt2.is(TokenKind::Greater));
    REQUIRE(gt2.lexeme == ">");

    // Generic depth drops back to 0, resuming normal mode
    REQUIRE_FALSE(lexer.isInGenericContext());

    Token eq = lexer.nextToken();
    REQUIRE(eq.is(TokenKind::Equal));
    REQUIRE(lexer.nextToken().is(TokenKind::LBracket));
    REQUIRE(lexer.nextToken().is(TokenKind::RBracket));
    REQUIRE(lexer.nextToken().is(TokenKind::Semicolon));
    REQUIRE(lexer.nextToken().is(TokenKind::Eof));
}

TEST_CASE("Lexer generic type immediately followed by = without space (array<int>= [])", "[lexer]") {
    // Section 'Closing Angle Brackets in Type Contexts (> vs. >= / >>)'
    std::string_view code = "array<int>= [];";
    Lexer lexer(1, code);
    lexer.enterGenericContext();

    REQUIRE(lexer.nextToken().is(TokenKind::KwArray));
    REQUIRE(lexer.nextToken().is(TokenKind::Less));
    REQUIRE(lexer.nextToken().is(TokenKind::KwInt));

    // In type context, '>' does NOT combine with '=' into '>='
    Token gt = lexer.nextToken();
    REQUIRE(gt.is(TokenKind::Greater));
    REQUIRE(gt.lexeme == ">");

    Token eq = lexer.nextToken();
    REQUIRE(eq.is(TokenKind::Equal));
    REQUIRE(eq.lexeme == "=");
}
