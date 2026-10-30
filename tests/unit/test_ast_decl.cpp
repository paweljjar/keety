#include <catch2/catch_test_macros.hpp>
#include <keety/ast/decl.hpp>

using namespace keety::ast;
using namespace keety::diag;

TEST_CASE("VarDecl and ConstDecl properties and immutability flags", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 15, 1, 16}};

    VarDecl varNode(span, "counter", nullptr, nullptr, false);
    REQUIRE(varNode.getName() == "counter");
    REQUIRE_FALSE(varNode.isExported());
    REQUIRE_FALSE(varNode.hasInitializer());

    VarDecl exportedVar(span, "activeRequests", nullptr, nullptr, true);
    REQUIRE(exportedVar.getName() == "activeRequests");
    REQUIRE(exportedVar.isExported());

    ConstDecl constNode(span, "MAX_ITEMS", nullptr, nullptr, true);
    REQUIRE(constNode.getName() == "MAX_ITEMS");
    REQUIRE(constNode.isExported());

    DefineDecl defineNode(span, "BUFFER_SIZE", nullptr, nullptr, false);
    REQUIRE(defineNode.getName() == "BUFFER_SIZE");
    REQUIRE_FALSE(defineNode.isExported());
}

TEST_CASE("FnDecl with parameters and async modifier", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 30, 1, 31}};

    Param p1{span, "data", nullptr, true, nullptr}; // mut data: ...
    Param p2{span, "scale", nullptr, false, nullptr};
    std::vector<Param> params;
    params.push_back(std::move(p1));
    params.push_back(std::move(p2));

    FnDecl fnNode(span, "transform", std::move(params), nullptr, nullptr, true, true, Visibility::Pub);

    REQUIRE(fnNode.getName() == "transform");
    REQUIRE(fnNode.isAsync());
    REQUIRE(fnNode.isExported());
    REQUIRE(fnNode.getVisibility() == Visibility::Pub);
    REQUIRE(fnNode.getParams().size() == 2);
    REQUIRE(fnNode.getParams()[0].name == "data");
    REQUIRE(fnNode.getParams()[0].isMut);
    REQUIRE_FALSE(fnNode.getParams()[1].isMut);
}

TEST_CASE("ClassDecl, EnumDecl, and ErrTypeDecl properties", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 20, 1, 21}};

    // Class with inheritance
    ClassDecl cls(span, "Player", "Entity", {}, {}, true);
    REQUIRE(cls.getName() == "Player");
    REQUIRE(cls.hasBaseClass());
    REQUIRE(cls.getBaseClass() == "Entity");
    REQUIRE(cls.isExported());

    // Enum
    std::vector<EnumMember> members;
    members.push_back(EnumMember{span, "OK", nullptr});
    members.push_back(EnumMember{span, "NotFound", nullptr});
    EnumDecl enumNode(span, "Status", nullptr, std::move(members), false);
    REQUIRE(enumNode.getName() == "Status");
    REQUIRE(enumNode.getMembers().size() == 2);
    REQUIRE(enumNode.getMembers()[0].name == "OK");

    // ErrType
    ErrTypeDecl errNode(span, "TimeoutError", "error", {}, true);
    REQUIRE(errNode.getName() == "TimeoutError");
    REQUIRE(errNode.getBaseError() == "error");
    REQUIRE(errNode.isExported());
}

TEST_CASE("ImportDecl selective and aliased symbols", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 25, 1, 26}};

    ImportedSymbol s1{"add", "", span};
    ImportedSymbol s2{"sub", "subtract", span};
    std::vector<ImportedSymbol> symbols;
    symbols.push_back(s1);
    symbols.push_back(s2);

    ImportDecl imp(span, "./math", "", std::move(symbols), false);
    REQUIRE(imp.getModulePath() == "./math");
    REQUIRE(imp.isSelective());
    REQUIRE(imp.getSymbols().size() == 2);
    REQUIRE(imp.getSymbols()[0].getEffectiveName() == "add");
    REQUIRE(imp.getSymbols()[1].getEffectiveName() == "subtract");
}
