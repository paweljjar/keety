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
