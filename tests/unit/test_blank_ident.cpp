#include <catch2/catch_test_macros.hpp>
#include <keety/lexer/lexer.hpp>

using namespace keety::lexer;

TEST_CASE("Lexer disambiguates standalone blank identifier from underscore identifiers", "[lexer]") {
    // 1. Standalone blank identifier in assignment and for-loops
    // let _ = compute();
    // for (const _, val in map)
    std::string_view code1 = "let _ = 42; let _count = 10; let __internal = 20; let _123 = 30;";
    Lexer lexer1(1, code1);

    auto tokens = lexer1.tokenizeAll();
    // 0: let, 1: _, 2: =, 3: 42, 4: ;
    // 5: let, 6: _count, 7: =, 8: 10, 9: ;
    // 10: let, 11: __internal, 12: =, 13: 20, 14: ;
    // 15: let, 16: _123, 17: =, 18: 30, 19: ;
    // 20: EOF
    REQUIRE(tokens.size() == 21);

    // Standalone '_' produces TokenKind::BlankIdent
    REQUIRE(tokens[1].is(TokenKind::BlankIdent));
    REQUIRE(tokens[1].lexeme == "_");

    // '_count' produces TokenKind::Identifier
    REQUIRE(tokens[6].is(TokenKind::Identifier));
    REQUIRE(tokens[6].lexeme == "_count");

    // '__internal' produces TokenKind::Identifier
    REQUIRE(tokens[11].is(TokenKind::Identifier));
    REQUIRE(tokens[11].lexeme == "__internal");

    // '_123' produces TokenKind::Identifier
    REQUIRE(tokens[16].is(TokenKind::Identifier));
    REQUIRE(tokens[16].lexeme == "_123");
}

TEST_CASE("Lexer blank identifier with delimiters and operators", "[lexer]") {
    // (_, y)
    std::string_view code2 = "(_, _foo, _)";
    Lexer lexer2(1, code2);

    auto tokens = lexer2.tokenizeAll();
    // 0: (, 1: _, 2: ,, 3: _foo, 4: ,, 5: _, 6: ), 7: EOF
    REQUIRE(tokens.size() == 8);

    REQUIRE(tokens[0].is(TokenKind::LParen));
    REQUIRE(tokens[1].is(TokenKind::BlankIdent));
    REQUIRE(tokens[2].is(TokenKind::Comma));
    REQUIRE(tokens[3].is(TokenKind::Identifier));
    REQUIRE(tokens[3].lexeme == "_foo");
    REQUIRE(tokens[4].is(TokenKind::Comma));
    REQUIRE(tokens[5].is(TokenKind::BlankIdent));
    REQUIRE(tokens[6].is(TokenKind::RParen));
    REQUIRE(tokens[7].is(TokenKind::Eof));
}
