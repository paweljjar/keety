#include <catch2/catch_test_macros.hpp>
#include <keety/fmt/comment_attacher.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;

TEST_CASE("Lexer captures single-line and multi-line comments with metadata", "[fmt][comment]") {
    std::string_view code =
        "// Header comment\n"
        "\n"
        "/* Multi-line\n"
        "   block comment */\n"
        "let x: int = 10; // Trailing inline\n"
        "// Footer comment\n";

    lexer::Lexer lexer(1, code);
    lexer.setCaptureComments(true);
    auto tokens = lexer.tokenizeAll();
    const auto& comments = lexer.getCapturedComments();

    REQUIRE(comments.size() == 4);

    // Comment 0: // Header comment
    CHECK(comments[0].isLine());
    CHECK(comments[0].text == "// Header comment");
    CHECK(comments[0].span.start.line == 1);
    CHECK_FALSE(comments[0].hasPrecedingBlankLine);

    // Comment 1: /* Multi-line ... */
    CHECK(comments[1].isBlock());
    CHECK(comments[1].span.start.line == 3);
    CHECK(comments[1].hasPrecedingBlankLine); // Separated by blank line from line 1

    // Comment 2: // Trailing inline
    CHECK(comments[2].isLine());
    CHECK(comments[2].text == "// Trailing inline");
    CHECK(comments[2].span.start.line == 5);
    CHECK_FALSE(comments[2].hasPrecedingBlankLine);

    // Comment 3: // Footer comment
    CHECK(comments[3].isLine());
    CHECK(comments[3].text == "// Footer comment");
    CHECK(comments[3].span.start.line == 6);
    CHECK_FALSE(comments[3].hasPrecedingBlankLine);
}

TEST_CASE("CommentAttacher binds file header, leading, trailing, and footer comments", "[fmt][comment]") {
    std::string_view code =
        "// Copyright (c) 2026 Keety Project\n"
        "// Apache-2.0 License\n"
        "\n"
        "// Helper function to add two numbers\n"
        "fn add(a: int, b: int): int {\n"
        "    // Calculate sum\n"
        "    let result: int = a + b; // inline result\n"
        "    return result;\n"
        "}\n"
        "\n"
        "// End of file comment\n";

    lexer::Lexer lexer(1, code);
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer);
    auto moduleAst = parser.parseModule("test_module");
    const auto& comments = lexer.getCapturedComments();

    REQUIRE(comments.size() == 6);
    REQUIRE(moduleAst.declarations.size() == 1);

    auto commentMap = fmt::CommentAttacher::attach(moduleAst, comments);

    // Total comments attached must equal input comments
    CHECK(commentMap.totalAttachedCount() == 6);

    // File header: the two license comments at the top separated by a blank line
    const auto& headers = commentMap.getFileHeader();
    REQUIRE(headers.size() == 2);
    CHECK(headers[0].text == "// Copyright (c) 2026 Keety Project");
    CHECK(headers[1].text == "// Apache-2.0 License");

    // File footer: comment at the end of the file after all declarations
    const auto& footers = commentMap.getFileFooter();
    REQUIRE(footers.size() == 1);
    CHECK(footers[0].text == "// End of file comment");

    // FnDecl leading comment
    const auto* fnDecl = moduleAst.declarations[0].get();
    const auto& fnLeading = commentMap.getLeading(fnDecl);
    REQUIRE(fnLeading.size() == 1);
    CHECK(fnLeading[0].text == "// Helper function to add two numbers");

    // Inside function body
    const auto* fn = dynamic_cast<const ast::FnDecl*>(fnDecl);
    REQUIRE(fn != nullptr);
    const auto* body = dynamic_cast<const ast::BlockStmt*>(fn->getBody());
    REQUIRE(body != nullptr);
    REQUIRE(body->getStatements().size() == 2);

    // Statement 0 (let result): leading and trailing comments
    const auto* stmt0 = body->getStatements()[0].get();
    const auto& stmt0Leading = commentMap.getLeading(stmt0);
    REQUIRE(stmt0Leading.size() == 1);
    CHECK(stmt0Leading[0].text == "// Calculate sum");

    const auto& stmt0Trailing = commentMap.getTrailing(stmt0);
    REQUIRE(stmt0Trailing.size() == 1);
    CHECK(stmt0Trailing[0].text == "// inline result");

    // Statement 1 (return result): no comments
    const auto* stmt1 = body->getStatements()[1].get();
    CHECK(commentMap.getLeading(stmt1).empty());
    CHECK(commentMap.getTrailing(stmt1).empty());
}

TEST_CASE("CommentAttacher binds dangling comments in empty blocks and at block end", "[fmt][comment]") {
    std::string_view code =
        "fn emptyFn(): void {\n"
        "    // Dangling comment inside empty body\n"
        "}\n"
        "\n"
        "fn trailingInBlock(): void {\n"
        "    let x: int = 1;\n"
        "    // Dangling comment at the end of block\n"
        "}\n";

    lexer::Lexer lexer(1, code);
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer);
    auto moduleAst = parser.parseModule();
    const auto& comments = lexer.getCapturedComments();

    REQUIRE(comments.size() == 2);
    REQUIRE(moduleAst.declarations.size() == 2);

    auto commentMap = fmt::CommentAttacher::attach(moduleAst, comments);
    CHECK(commentMap.totalAttachedCount() == 2);

    // emptyFn body should receive dangling comment
    const auto* fn0 = dynamic_cast<const ast::FnDecl*>(moduleAst.declarations[0].get());
    REQUIRE(fn0 != nullptr);
    const auto* body0 = fn0->getBody();
    const auto& body0Dangling = commentMap.getDangling(body0);
    REQUIRE(body0Dangling.size() == 1);
    CHECK(body0Dangling[0].text == "// Dangling comment inside empty body");

    // trailingInBlock body should receive dangling comment at end of block
    const auto* fn1 = dynamic_cast<const ast::FnDecl*>(moduleAst.declarations[1].get());
    REQUIRE(fn1 != nullptr);
    const auto* body1 = fn1->getBody();
    const auto& body1Dangling = commentMap.getDangling(body1);
    REQUIRE(body1Dangling.size() == 1);
    CHECK(body1Dangling[0].text == "// Dangling comment at the end of block");
}

TEST_CASE("CommentAttacher binds comments inside classes and enums", "[fmt][comment]") {
    std::string_view code =
        "class Config {\n"
        "    // Timeout in milliseconds\n"
        "    pub let timeoutMs: int; // inline timeout\n"
        "\n"
        "    // Factory method\n"
        "    fn create(): Config {\n"
        "        // TODO: implement\n"
        "    }\n"
        "}\n"
        "\n"
        "enum Status: int {\n"
        "    // Success code\n"
        "    Ok = 200, // http 200\n"
        "    // Empty enum dangling comment before closing brace\n"
        "}\n";

    lexer::Lexer lexer(1, code);
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer);
    auto moduleAst = parser.parseModule();
    const auto& comments = lexer.getCapturedComments();

    REQUIRE(comments.size() == 7);
    REQUIRE(moduleAst.declarations.size() == 2);

    auto commentMap = fmt::CommentAttacher::attach(moduleAst, comments);
    CHECK(commentMap.totalAttachedCount() == 7);

    const auto* cls = dynamic_cast<const ast::ClassDecl*>(moduleAst.declarations[0].get());
    REQUIRE(cls != nullptr);
    REQUIRE(cls->getFields().size() == 1);
    REQUIRE(cls->getMethods().size() == 1);

    // Field comments
    const auto* field = &cls->getFields()[0];
    const auto& fieldLeading = commentMap.getLeading(field);
    REQUIRE(fieldLeading.size() == 1);
    CHECK(fieldLeading[0].text == "// Timeout in milliseconds");

    const auto& fieldTrailing = commentMap.getTrailing(field);
    REQUIRE(fieldTrailing.size() == 1);
    CHECK(fieldTrailing[0].text == "// inline timeout");

    // Method comments
    const auto* method = cls->getMethods()[0].get();
    const auto& methodLeading = commentMap.getLeading(method);
    REQUIRE(methodLeading.size() == 1);
    CHECK(methodLeading[0].text == "// Factory method");

    // Method body dangling comment
    const auto* methodBody = method->getBody();
    const auto& methodDangling = commentMap.getDangling(methodBody);
    REQUIRE(methodDangling.size() == 1);
    CHECK(methodDangling[0].text == "// TODO: implement");

    // Enum member comments
    const auto* enm = dynamic_cast<const ast::EnumDecl*>(moduleAst.declarations[1].get());
    REQUIRE(enm != nullptr);
    REQUIRE(enm->getMembers().size() == 1);

    const auto* member = &enm->getMembers()[0];
    const auto& memberLeading = commentMap.getLeading(member);
    REQUIRE(memberLeading.size() == 1);
    CHECK(memberLeading[0].text == "// Success code");

    const auto& memberTrailing = commentMap.getTrailing(member);
    REQUIRE(memberTrailing.size() == 1);
    CHECK(memberTrailing[0].text == "// http 200");

    // Enum body dangling comment
    const auto& enmDangling = commentMap.getDangling(enm);
    REQUIRE(enmDangling.size() == 1);
    CHECK(enmDangling[0].text == "// Empty enum dangling comment before closing brace");
}
