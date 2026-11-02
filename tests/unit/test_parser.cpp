#include <catch2/catch_test_macros.hpp>
#include <keety/parser/parser.hpp>

using namespace keety::parser;
using namespace keety::lexer;
using namespace keety::ast;

TEST_CASE("Parser parses variable and constant declarations", "[parser]") {
    std::string_view code = "let count: int = 10; const PI: float = 3.14; define SIZE: int = 100;";
    Lexer lexer(1, code);
    Parser parser(lexer);

    ModuleAST mod = parser.parseModule("test_decl");
    REQUIRE(mod.declarations.size() == 3);

    auto* varD = dynamic_cast<VarDecl*>(mod.declarations[0].get());
    REQUIRE(varD != nullptr);
    REQUIRE(varD->getName() == "count");
    REQUIRE(varD->getType() != nullptr);
    REQUIRE(varD->hasInitializer());

    auto* constD = dynamic_cast<ConstDecl*>(mod.declarations[1].get());
    REQUIRE(constD != nullptr);
    REQUIRE(constD->getName() == "PI");

    auto* defD = dynamic_cast<DefineDecl*>(mod.declarations[2].get());
    REQUIRE(defD != nullptr);
    REQUIRE(defD->getName() == "SIZE");
}

TEST_CASE("Parser parses function and class declarations", "[parser]") {
    std::string_view code =
        "fn calculate(a: int, mut b: int): int {\n"
        "    return a;\n"
        "}\n"
        "class Player inherits Entity {\n"
        "    pub let id: int;\n"
        "}\n";
    Lexer lexer(1, code);
    Parser parser(lexer);

    ModuleAST mod = parser.parseModule("test_fn_class");
    REQUIRE(mod.declarations.size() == 2);

    auto* fnD = dynamic_cast<FnDecl*>(mod.declarations[0].get());
    REQUIRE(fnD != nullptr);
    REQUIRE(fnD->getName() == "calculate");
    REQUIRE(fnD->getParams().size() == 2);
    REQUIRE(fnD->getParams()[1].isMut);
    REQUIRE(fnD->getBody() != nullptr);

    auto* clsD = dynamic_cast<ClassDecl*>(mod.declarations[1].get());
    REQUIRE(clsD != nullptr);
    REQUIRE(clsD->getName() == "Player");
    REQUIRE(clsD->hasBaseClass());
    REQUIRE(clsD->getBaseClass() == "Entity");
    REQUIRE(clsD->getFields().size() == 1);
    REQUIRE(clsD->getFields()[0].name == "id");
}

TEST_CASE("Parser parses union types and collections", "[parser]") {
    std::string_view code = "let val: int | string | null = 42; let list: array<int>;";
    Lexer lexer(1, code);
    Parser parser(lexer);

    ModuleAST mod = parser.parseModule("test_types");
    REQUIRE(mod.declarations.size() == 2);

    auto* var1 = dynamic_cast<VarDecl*>(mod.declarations[0].get());
    REQUIRE(var1 != nullptr);
    auto* unionT = dynamic_cast<const UnionType*>(var1->getType());
    REQUIRE(unionT != nullptr);
    REQUIRE(unionT->getVariants().size() == 3);

    auto* var2 = dynamic_cast<VarDecl*>(mod.declarations[1].get());
    REQUIRE(var2 != nullptr);
    auto* arrT = dynamic_cast<const ArrayType*>(var2->getType());
    REQUIRE(arrT != nullptr);
}

TEST_CASE("Parser correctly parses binary subtraction following identifier without false-positive E0015", "[parser]") {
    std::string_view code =
        "fn test(indexOneBased: int): void {\n"
        "    let i: int = 0;\n"
        "    if (i == indexOneBased - 1) {\n"
        "        return;\n"
        "    }\n"
        "}\n"
        "fn test2(a: int): void {\n"
        "    let x: int = a - 1;\n"
        "}\n";

    keety::diag::SourceManager sm;
    auto fid = sm.addBuffer("test.keet", std::string(code));
    keety::diag::DiagnosticEmitter emitter(sm, keety::diag::ColorMode::Never);
    Lexer lexer(fid, code, &emitter);
    Parser parser(lexer, &emitter);

    ModuleAST mod = parser.parseModule("test_binary_subtraction");
    REQUIRE_FALSE(emitter.hasErrors());
    REQUIRE(emitter.getDiagnostics().empty());
    REQUIRE(mod.declarations.size() == 2);

    auto* fn1 = dynamic_cast<FnDecl*>(mod.declarations[0].get());
    REQUIRE(fn1 != nullptr);
    REQUIRE(fn1->getName() == "test");
    auto* body1 = dynamic_cast<const BlockStmt*>(fn1->getBody());
    REQUIRE(body1 != nullptr);
    const auto& stmts1 = body1->getStatements();
    REQUIRE(stmts1.size() == 2);
    auto* ifStmt = dynamic_cast<IfStmt*>(stmts1[1].get());
    REQUIRE(ifStmt != nullptr);
    auto* cond = dynamic_cast<const BinaryExpr*>(ifStmt->getCondition());
    REQUIRE(cond != nullptr);
    REQUIRE(cond->getOp() == TokenKind::EqualEqual);
    auto* rightSub = dynamic_cast<const BinaryExpr*>(cond->getRight());
    REQUIRE(rightSub != nullptr);
    REQUIRE(rightSub->getOp() == TokenKind::Minus);

    auto* fn2 = dynamic_cast<FnDecl*>(mod.declarations[1].get());
    REQUIRE(fn2 != nullptr);
    REQUIRE(fn2->getName() == "test2");
    auto* body2 = dynamic_cast<const BlockStmt*>(fn2->getBody());
    REQUIRE(body2 != nullptr);
    const auto& stmts2 = body2->getStatements();
    REQUIRE(stmts2.size() == 1);
    auto* declStmt = dynamic_cast<DeclStmt*>(stmts2[0].get());
    REQUIRE(declStmt != nullptr);
    auto* varDecl = dynamic_cast<const VarDecl*>(declStmt->getDeclaration());
    REQUIRE(varDecl != nullptr);
    REQUIRE(varDecl->getName() == "x");
    REQUIRE(varDecl->hasInitializer());
    auto* initSub = dynamic_cast<const BinaryExpr*>(varDecl->getInitializer());
    REQUIRE(initSub != nullptr);
    REQUIRE(initSub->getOp() == TokenKind::Minus);
}

