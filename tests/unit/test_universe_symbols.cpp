#include <catch2/catch_test_macros.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/symbol.hpp>
#include <keety/sema/universe.hpp>

using namespace keety::sema;

TEST_CASE("Predeclared Universe types inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    const std::vector<std::string> expectedTypes = {
        "bool", "int", "float", "char", "string", "void", "null", "error", "array", "map", "never"
    };

    for (const auto& typeName : expectedTypes) {
        DYNAMIC_SECTION("Type: " << typeName) {
            CHECK(isUniverseType(typeName));
            CHECK(isUniverseSymbol(typeName));

            auto sym = universe->lookupCurrent(typeName);
            REQUIRE(sym != nullptr);
            CHECK(sym->getName() == typeName);
            CHECK(sym->isType());
            CHECK(sym->isBuiltin());
            CHECK(sym->isBuiltinType());
        }
    }

    CHECK_FALSE(isUniverseType("double"));
    CHECK_FALSE(isUniverseType("uint"));
    CHECK_FALSE(isUniverseType("any"));
    CHECK_FALSE(isUniverseType("CustomClass"));
}

TEST_CASE("Predeclared Universe constants inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    SECTION("true constant") {
        CHECK(isUniverseConstant("true"));
        CHECK(isUniverseSymbol("true"));

        auto trueSym = universe->lookupCurrent("true");
        REQUIRE(trueSym != nullptr);
        CHECK(trueSym->isConstant());
        CHECK(trueSym->isBuiltinConstant());
        CHECK(trueSym->isBuiltin());
        CHECK_FALSE(trueSym->isMutable());
    }

    SECTION("false constant") {
        CHECK(isUniverseConstant("false"));
        CHECK(isUniverseSymbol("false"));

        auto falseSym = universe->lookupCurrent("false");
        REQUIRE(falseSym != nullptr);
        CHECK(falseSym->isConstant());
        CHECK(falseSym->isBuiltinConstant());
        CHECK(falseSym->isBuiltin());
        CHECK_FALSE(falseSym->isMutable());
    }

    SECTION("null is both constant and type") {
        CHECK(isUniverseConstant("null"));
        CHECK(isUniverseType("null"));
        CHECK(isUniverseSymbol("null"));

        auto nullSym = universe->lookupCurrent("null");
        REQUIRE(nullSym != nullptr);
        CHECK(nullSym->isConstant());
        CHECK(nullSym->isType());
        CHECK(nullSym->isBuiltin());
    }

    CHECK_FALSE(isUniverseConstant("undefined"));
    CHECK_FALSE(isUniverseConstant("nil"));
    CHECK_FALSE(isUniverseConstant("None"));
}

TEST_CASE("Predeclared Universe operators inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    SECTION("typeof operator") {
        CHECK(isUniverseOperator("typeof"));
        CHECK(isUniverseSymbol("typeof"));

        auto typeofSym = universe->lookupCurrent("typeof");
        REQUIRE(typeofSym != nullptr);
        CHECK(typeofSym->isBuiltinOperator());
        CHECK(typeofSym->isBuiltin());
        CHECK(typeofSym->getKind() == SymbolKind::BuiltinOperator);
    }

    CHECK_FALSE(isUniverseOperator("sizeof"));
    CHECK_FALSE(isUniverseOperator("alignof"));
}

TEST_CASE("Universe catalog lists and queries", "[sema][universe]") {
    const auto& types = getUniverseTypeNames();
    CHECK(types.size() == 11);

    const auto& constants = getUniverseConstantNames();
    CHECK(constants.size() == 3);

    const auto& all = getAllUniverseSymbolNames();
    CHECK(all.size() == 14);

    auto universe = Scope::createUniverse();
    CHECK(universe->size() == 14);
}

TEST_CASE("Universe symbols propagate down through child scopes", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    auto fnScope = moduleScope->createChild(ScopeKind::Function);
    auto blockScope = fnScope->createChild(ScopeKind::Block);

    SECTION("Resolving built-ins from innermost block scope") {
        auto intSym = blockScope->lookup("int");
        REQUIRE(intSym != nullptr);
        CHECK(intSym->isBuiltinType());

        auto trueSym = blockScope->lookup("true");
        REQUIRE(trueSym != nullptr);
        CHECK(trueSym->isBuiltinConstant());

        auto typeofSym = blockScope->lookup("typeof");
        REQUIRE(typeofSym != nullptr);
        CHECK(typeofSym->isBuiltinOperator());

        // Not present in local block scope directly
        CHECK(blockScope->lookupCurrent("int") == nullptr);
        CHECK(blockScope->lookupParent("int") == intSym);
    }

    SECTION("Universe symbols are detected as shadowing targets") {
        auto candidateInt = fnScope->lookupParentForShadowing("int");
        REQUIRE(candidateInt != nullptr);
        CHECK(candidateInt->getName() == "int");

        auto candidateTypeof = blockScope->lookupParentForShadowing("typeof");
        REQUIRE(candidateTypeof != nullptr);
        CHECK(candidateTypeof->getName() == "typeof");

        auto candidateTrue = moduleScope->lookupParentForShadowing("true");
        REQUIRE(candidateTrue != nullptr);
        CHECK(candidateTrue->getName() == "true");
    }

    SECTION("Empty universe without predeclared symbols when requested") {
        auto emptyUniverse = Scope::createUniverse(/*populatePredeclared=*/false);
        CHECK(emptyUniverse->empty());
        CHECK(emptyUniverse->lookup("int") == nullptr);
    }
}
