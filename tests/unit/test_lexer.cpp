#include <catch2/catch_test_macros.hpp>
#include <keety/lexer/lexer.hpp>

using namespace keety::lexer;

TEST_CASE("Lexer scans simple declaration and Maximal Munch operators", "[lexer]") {
    std::string_view code = "let count: int = 42; count += 10; ++count;";
    Lexer lexer(1, code);

    // Input: "let count: int = 42; count += 10; ++count;"
    // 0: let, 1: count, 2: :, 3: int, 4: =, 5: 42, 6: ;
    // 7: count, 8: +=, 9: 10, 10: ;
    // 11: ++, 12: count, 13: ;, 14: EOF
    auto tokens = lexer.tokenizeAll();
    REQUIRE(tokens.size() == 15);

    REQUIRE(tokens[0].is(TokenKind::KwLet));
    REQUIRE(tokens[1].is(TokenKind::Identifier));
    REQUIRE(tokens[1].lexeme == "count");
    REQUIRE(tokens[2].is(TokenKind::Colon));
    REQUIRE(tokens[3].is(TokenKind::KwInt));
    REQUIRE(tokens[4].is(TokenKind::Equal));
    REQUIRE(tokens[5].is(TokenKind::IntLiteral));
    REQUIRE(tokens[5].lexeme == "42");
    REQUIRE(tokens[6].is(TokenKind::Semicolon));

    REQUIRE(tokens[7].is(TokenKind::Identifier));
    REQUIRE(tokens[8].is(TokenKind::PlusEqual));
    REQUIRE(tokens[9].is(TokenKind::IntLiteral));
    REQUIRE(tokens[10].is(TokenKind::Semicolon));

    REQUIRE(tokens[11].is(TokenKind::PlusPlus));
    REQUIRE(tokens[12].is(TokenKind::Identifier));
    REQUIRE(tokens[13].is(TokenKind::Semicolon));
    REQUIRE(tokens[14].is(TokenKind::Eof));
}

TEST_CASE("Lexer skips single-line and multi-line comments", "[lexer]") {
    std::string_view code =
        "// Line comment\n"
        "/* Multi-line\n"
        "   comment */\n"
        "const pi: float = 3.1415;";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    REQUIRE(tokens[0].is(TokenKind::KwConst));
    REQUIRE(tokens[1].is(TokenKind::Identifier));
    REQUIRE(tokens[1].lexeme == "pi");
    REQUIRE(tokens[2].is(TokenKind::Colon));
    REQUIRE(tokens[3].is(TokenKind::KwFloat));
    REQUIRE(tokens[4].is(TokenKind::Equal));
    REQUIRE(tokens[5].is(TokenKind::FloatLiteral));
    REQUIRE(tokens[5].lexeme == "3.1415");
    REQUIRE(tokens[6].is(TokenKind::Semicolon));
    REQUIRE(tokens[7].is(TokenKind::Eof));
}

TEST_CASE("Lexer numeric bases (binary, octal, hex)", "[lexer]") {
    std::string_view code = "0b1010 0o755 0xBadFace 1.5e-10";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    REQUIRE(tokens[0].is(TokenKind::IntLiteral));
    REQUIRE(tokens[0].lexeme == "0b1010");
    REQUIRE(tokens[1].is(TokenKind::IntLiteral));
    REQUIRE(tokens[1].lexeme == "0o755");
    REQUIRE(tokens[2].is(TokenKind::IntLiteral));
    REQUIRE(tokens[2].lexeme == "0xBadFace");
    REQUIRE(tokens[3].is(TokenKind::FloatLiteral));
    REQUIRE(tokens[3].lexeme == "1.5e-10");
}

TEST_CASE("Lexer reports error for unterminated block comment", "[lexer]") {
    keety::diag::SourceManager sm;
    keety::diag::DiagnosticEmitter emitter(sm);
    std::string_view code = "let x = 1; /* unclosed comment";
    Lexer lexer(1, code, &emitter);

    auto tokens = lexer.tokenizeAll();
    REQUIRE(emitter.hasErrors());
    REQUIRE(emitter.getErrorCount() == 1);
    REQUIRE(emitter.getDiagnostics()[0].code == "E0002");
}
