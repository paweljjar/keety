#include <catch2/catch_test_macros.hpp>
#include <keety/lexer/token.hpp>

using namespace keety::lexer;

TEST_CASE("TokenKind names and classification", "[lexer]") {
    // Basic identifiers & literals
    REQUIRE(getTokenKindName(TokenKind::Identifier) == "identifier");
    REQUIRE(getTokenKindName(TokenKind::BlankIdent) == "_");
    REQUIRE(getTokenKindName(TokenKind::IntLiteral) == "integer literal");

    // Keywords
    REQUIRE(isKeyword(TokenKind::KwLet));
    REQUIRE(isKeyword(TokenKind::KwMut));
    REQUIRE(isKeyword(TokenKind::KwFn));
    REQUIRE(isKeyword(TokenKind::KwZonk));
    REQUIRE(isKeyword(TokenKind::KwAsync));
    REQUIRE(isKeyword(TokenKind::KwAwait));
    REQUIRE_FALSE(isKeyword(TokenKind::Identifier));
    REQUIRE_FALSE(isKeyword(TokenKind::Plus));

    // Literals
    REQUIRE(isLiteral(TokenKind::IntLiteral));
    REQUIRE(isLiteral(TokenKind::FloatLiteral));
    REQUIRE(isLiteral(TokenKind::StringLiteral));
    REQUIRE(isLiteral(TokenKind::KwTrue));
    REQUIRE(isLiteral(TokenKind::KwFalse));
    REQUIRE(isLiteral(TokenKind::KwNull));
    REQUIRE_FALSE(isLiteral(TokenKind::KwLet));

    // Assignment operators
    REQUIRE(isAssignmentOperator(TokenKind::Equal));
    REQUIRE(isAssignmentOperator(TokenKind::PlusEqual));
    REQUIRE(isAssignmentOperator(TokenKind::MinusEqual));
    REQUIRE(isAssignmentOperator(TokenKind::StarEqual));
    REQUIRE_FALSE(isAssignmentOperator(TokenKind::EqualEqual));
    REQUIRE_FALSE(isAssignmentOperator(TokenKind::Plus));
}

TEST_CASE("Token methods and comparisons", "[lexer]") {
    keety::diag::SourceLocation loc{1, 0, 1, 1};
    keety::diag::SourceSpan span{loc, loc};
    Token tok{TokenKind::KwLet, span, "let"};

    REQUIRE(tok.is(TokenKind::KwLet));
    REQUIRE(tok.isNot(TokenKind::KwConst));
    REQUIRE(tok.isOneOf(TokenKind::KwConst, TokenKind::KwLet));
    REQUIRE(tok.isOneOf(TokenKind::KwDefine, TokenKind::KwConst, TokenKind::KwLet));
    REQUIRE(tok.lexeme == "let");
}
