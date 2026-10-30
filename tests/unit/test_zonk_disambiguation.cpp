#include <catch2/catch_test_macros.hpp>
#include <keety/parser/parser.hpp>

using namespace keety::parser;
using namespace keety::lexer;
using namespace keety::ast;

TEST_CASE("Parser disambiguates zonk fallback expression vs scoped recovery block", "[parser]") {
    SECTION("Fallback value form with numeric literal") {
        std::string_view code = "raw.toInt() zonk 8080;";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());
        REQUIRE(zonk->getPrimary() != nullptr);
        REQUIRE(zonk->getFallback() != nullptr);

        auto* fallbackLit = dynamic_cast<const LiteralExpr*>(zonk->getFallback());
        REQUIRE(fallbackLit != nullptr);
        REQUIRE(fallbackLit->getLiteralKind() == TokenKind::IntLiteral);
        REQUIRE(fallbackLit->getRawText() == "8080");
    }

    SECTION("Fallback value form with empty map literal ({})") {
        std::string_view code = "loadScores() zonk {};";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());
        REQUIRE(zonk->getPrimary() != nullptr);
        REQUIRE(zonk->getFallback() != nullptr);

        auto* mapLit = dynamic_cast<const MapLitExpr*>(zonk->getFallback());
        REQUIRE(mapLit != nullptr);
        REQUIRE(mapLit->getEntries().empty());
    }

    SECTION("Fallback value form with parenthesized expression") {
        std::string_view code = "fetch() zonk (fallbackVal);";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());

        auto* fallbackIdent = dynamic_cast<const IdentExpr*>(zonk->getFallback());
        REQUIRE(fallbackIdent != nullptr);
        REQUIRE(fallbackIdent->getName() == "fallbackVal");
    }

    SECTION("Fallback value form with binary expression looser precedence") {
        std::string_view code = "a zonk b + c;";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());

        // fallback should be `b + c`
        auto* binary = dynamic_cast<const BinaryExpr*>(zonk->getFallback());
        REQUIRE(binary != nullptr);
        REQUIRE(binary->getOp() == TokenKind::Plus);
    }

    SECTION("Scoped recovery block with error identifier") {
        std::string_view code = "readPort() zonk (err) { yield 8080; };";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE(zonk->hasRecoveryBlock());
        REQUIRE(zonk->getFallback() == nullptr);
        REQUIRE(zonk->getErrorVarName() == "err");

        const auto* block = zonk->getRecoveryBlock();
        REQUIRE(block != nullptr);
        REQUIRE(block->getStatements().size() == 1);

        auto* yieldStmt = dynamic_cast<const YieldStmt*>(block->getStatements()[0].get());
        REQUIRE(yieldStmt != nullptr);
        REQUIRE(yieldStmt->getValue() != nullptr);
    }

    SECTION("Scoped recovery block without error identifier (empty parens)") {
        std::string_view code = "readConfig() zonk () { return; };";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE(zonk->hasRecoveryBlock());
        REQUIRE(zonk->getFallback() == nullptr);
        REQUIRE(zonk->getErrorVarName().empty());

        const auto* block = zonk->getRecoveryBlock();
        REQUIRE(block != nullptr);
        REQUIRE(block->getStatements().size() == 1);

        auto* retStmt = dynamic_cast<const ReturnStmt*>(block->getStatements()[0].get());
        REQUIRE(retStmt != nullptr);
        REQUIRE(retStmt->getValue() == nullptr);
    }

    SECTION("Scoped recovery block with blank identifier (_)") {
        std::string_view code = "readConfig() zonk (_) { return; };";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE(zonk->hasRecoveryBlock());
        REQUIRE(zonk->getFallback() == nullptr);
        REQUIRE(zonk->getErrorVarName() == "_");
    }

    SECTION("Chained zonk expressions associate left-to-right") {
        std::string_view code = "a zonk b zonk c;";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        // (a zonk b) zonk c
        auto* outerZonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(outerZonk != nullptr);

        auto* innerZonk = dynamic_cast<const ZonkExpr*>(outerZonk->getPrimary());
        REQUIRE(innerZonk != nullptr);

        auto* cIdent = dynamic_cast<const IdentExpr*>(outerZonk->getFallback());
        REQUIRE(cIdent != nullptr);
        REQUIRE(cIdent->getName() == "c");

        auto* aIdent = dynamic_cast<const IdentExpr*>(innerZonk->getPrimary());
        REQUIRE(aIdent != nullptr);
        REQUIRE(aIdent->getName() == "a");

        auto* bIdent = dynamic_cast<const IdentExpr*>(innerZonk->getFallback());
        REQUIRE(bIdent != nullptr);
        REQUIRE(bIdent->getName() == "b");
    }

    SECTION("Fallback value form with non-empty map literal") {
        std::string_view code = "loadScores() zonk {\"default\": 1};";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());

        auto* mapLit = dynamic_cast<const MapLitExpr*>(zonk->getFallback());
        REQUIRE(mapLit != nullptr);
        REQUIRE(mapLit->getEntries().size() == 1);
    }

    SECTION("Parenthesized fallback expression followed by higher-precedence operator") {
        std::string_view code = "fetch() zonk (a + b) * 2;";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());

        // fallback is `(a + b) * 2`
        auto* mulBinary = dynamic_cast<const BinaryExpr*>(zonk->getFallback());
        REQUIRE(mulBinary != nullptr);
        REQUIRE(mulBinary->getOp() == TokenKind::Star);
    }

    SECTION("Parenthesized identifier fallback when no brace follows") {
        std::string_view code = "fetch() zonk (err);";
        Lexer lexer(1, code);
        Parser parser(lexer);

        auto stmt = parser.parseStatement();
        auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get());
        REQUIRE(exprStmt != nullptr);

        auto* zonk = dynamic_cast<const ZonkExpr*>(exprStmt->getExpression());
        REQUIRE(zonk != nullptr);
        REQUIRE_FALSE(zonk->hasRecoveryBlock());

        auto* ident = dynamic_cast<const IdentExpr*>(zonk->getFallback());
        REQUIRE(ident != nullptr);
        REQUIRE(ident->getName() == "err");
    }
}

