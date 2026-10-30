#include <catch2/catch_test_macros.hpp>
#include <keety/lexer/lexer.hpp>

using namespace keety::lexer;

TEST_CASE("Lexer simple template string without substitutions", "[lexer]") {
    std::string_view code = "`Hello, world!`;";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    REQUIRE(tokens.size() == 3); // TemplateNoSub, Semicolon, Eof
    REQUIRE(tokens[0].is(TokenKind::TemplateNoSub));
    REQUIRE(tokens[0].lexeme == "`Hello, world!`");
    REQUIRE(tokens[1].is(TokenKind::Semicolon));
    REQUIRE(tokens[2].is(TokenKind::Eof));
}

TEST_CASE("Lexer multiline template literal with simple interpolation", "[lexer]") {
    // Multiline template with interpolation:
    // `Line 1: ${name}
    //  Line 2!`
    std::string_view code = "`Line 1: ${name}\n Line 2!`;";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    // 0: TemplateHead (`Line 1: ${)
    // 1: Identifier (name)
    // 2: TemplateTail (\n Line 2!`)
    // 3: Semicolon (;)
    // 4: Eof
    REQUIRE(tokens.size() == 5);

    REQUIRE(tokens[0].is(TokenKind::TemplateHead));
    REQUIRE(tokens[0].lexeme == "`Line 1: ${");

    REQUIRE(tokens[1].is(TokenKind::Identifier));
    REQUIRE(tokens[1].lexeme == "name");

    REQUIRE(tokens[2].is(TokenKind::TemplateTail));
    REQUIRE(tokens[2].lexeme == "\n Line 2!`");

    REQUIRE(tokens[3].is(TokenKind::Semicolon));
    REQUIRE(tokens[4].is(TokenKind::Eof));
}

TEST_CASE("Lexer template with multiple interpolations and expressions", "[lexer]") {
    // `X: ${x}, Y: ${y + 10}!`
    std::string_view code = "`X: ${x}, Y: ${y + 10}!`;";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    // 0: TemplateHead (`X: ${)
    // 1: x
    // 2: TemplateMiddle (, Y: ${)
    // 3: y
    // 4: +
    // 5: 10
    // 6: TemplateTail (!`)
    // 7: ;
    // 8: Eof
    REQUIRE(tokens.size() == 9);

    REQUIRE(tokens[0].is(TokenKind::TemplateHead));
    REQUIRE(tokens[0].lexeme == "`X: ${");

    REQUIRE(tokens[1].is(TokenKind::Identifier));
    REQUIRE(tokens[1].lexeme == "x");

    REQUIRE(tokens[2].is(TokenKind::TemplateMiddle));
    REQUIRE(tokens[2].lexeme == ", Y: ${");

    REQUIRE(tokens[3].is(TokenKind::Identifier));
    REQUIRE(tokens[3].lexeme == "y");

    REQUIRE(tokens[4].is(TokenKind::Plus));

    REQUIRE(tokens[5].is(TokenKind::IntLiteral));
    REQUIRE(tokens[5].lexeme == "10");

    REQUIRE(tokens[6].is(TokenKind::TemplateTail));
    REQUIRE(tokens[6].lexeme == "!`");

    REQUIRE(tokens[7].is(TokenKind::Semicolon));
}

TEST_CASE("Lexer template with nested braces inside interpolation", "[lexer]") {
    // Expression inside interpolation contains block/braces:
    // `${ fn(): int { return 42; }() }`
    std::string_view code = "`${ fn(): int { return 42; }() }`;";
    Lexer lexer(1, code);

    auto tokens = lexer.tokenizeAll();
    // 0: TemplateHead (`${)
    // 1: fn
    // 2: (
    // 3: )
    // 4: :
    // 5: int
    // 6: {
    // 7: return
    // 8: 42
    // 9: ;
    // 10: }
    // 11: (
    // 12: )
    // 13: TemplateTail (`)
    // 14: ;
    // 15: Eof
    REQUIRE(tokens.size() == 16);

    REQUIRE(tokens[0].is(TokenKind::TemplateHead));
    REQUIRE(tokens[1].is(TokenKind::KwFn));
    REQUIRE(tokens[6].is(TokenKind::LBrace));
    REQUIRE(tokens[10].is(TokenKind::RBrace)); // Inner block brace doesn't close template
    REQUIRE(tokens[13].is(TokenKind::TemplateTail));
}

TEST_CASE("Lexer reports error for unterminated template literal", "[lexer]") {
    keety::diag::SourceManager sm;
    keety::diag::DiagnosticEmitter emitter(sm);
    std::string_view code = "let s = `unterminated template;";
    Lexer lexer(1, code, &emitter);

    auto tokens = lexer.tokenizeAll();
    REQUIRE(emitter.hasErrors());
    REQUIRE(emitter.getErrorCount() == 1);
    REQUIRE(emitter.getDiagnostics()[0].code == "E0001");
}
