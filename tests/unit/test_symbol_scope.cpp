#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_location.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/symbol.hpp>

using namespace keety;
using namespace keety::sema;

TEST_CASE("Symbol properties and predicates", "[sema][symbol]") {
    diag::SourceSpan dummySpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 10, 9}};

    SECTION("Variable symbol") {
        auto sym = std::make_shared<Symbol>("count", SymbolKind::Variable, dummySpan);
        CHECK(sym->getName() == "count");
        CHECK(sym->getKind() == SymbolKind::Variable);
        CHECK(sym->getKindName() == "variable");
        CHECK(sym->isVariable());
        CHECK(sym->isValueBinding());
        CHECK_FALSE(sym->isConstant());
        CHECK_FALSE(sym->isFunction());
        CHECK_FALSE(sym->isExported());
        CHECK_FALSE(sym->isMutable());

        sym->setMutable(true);
        sym->setExported(true);
        CHECK(sym->isMutable());
        CHECK(sym->isExported());
    }

    SECTION("Constant and Define symbols") {
        auto constSym = std::make_shared<Symbol>("PI", SymbolKind::Constant, dummySpan);
        CHECK(constSym->isConstant());
        CHECK(constSym->isValueBinding());
        CHECK_FALSE(constSym->isDefine());

        auto defSym = std::make_shared<Symbol>("BUFFER_SIZE", SymbolKind::Define, dummySpan);
        CHECK(defSym->isConstant());
        CHECK(defSym->isDefine());
        CHECK(defSym->isValueBinding());
    }

    SECTION("Function symbol and overloading") {
        auto fn1 = std::make_shared<Symbol>("render", SymbolKind::Function, dummySpan);
        auto fn2 = std::make_shared<Symbol>("render", SymbolKind::Function, dummySpan);

        CHECK(fn1->isFunction());
        CHECK_FALSE(fn1->hasOverloads());
        CHECK(fn1->getOverloads().empty());

        fn1->addOverload(fn2);
        CHECK(fn1->hasOverloads());
        REQUIRE(fn1->getOverloads().size() == 1);
        CHECK(fn1->getOverloads()[0] == fn2);
    }

    SECTION("Type symbols and builtins") {
        auto classSym = std::make_shared<Symbol>("Widget", SymbolKind::Class, dummySpan);
        CHECK(classSym->isClass());
        CHECK(classSym->isType());
        CHECK_FALSE(classSym->isValueBinding());

        auto builtinSym = std::make_shared<Symbol>("int", SymbolKind::BuiltinType, dummySpan);
        CHECK(builtinSym->isBuiltin());
        CHECK(builtinSym->isType());

        auto typeofSym = std::make_shared<Symbol>("typeof", SymbolKind::BuiltinOperator, dummySpan);
        CHECK(typeofSym->isBuiltin());
    }

    SECTION("Const view for exported variables") {
        auto impSym = std::make_shared<Symbol>("activeUsers", SymbolKind::Variable, dummySpan);
        impSym->setConstView(true);
        CHECK(impSym->isConstView());
    }
}

TEST_CASE("Hierarchical Scope tree construction", "[sema][scope]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);
    CHECK(universe->getKind() == ScopeKind::Universe);
    CHECK(universe->isUniverse());
    CHECK(universe->getParent() == nullptr);
    CHECK(universe->getDepth() == 0);
    CHECK(universe->getKindName() == "Universe");

    auto moduleScope = universe->createChild(ScopeKind::Module);
    REQUIRE(moduleScope != nullptr);
    CHECK(moduleScope->getKind() == ScopeKind::Module);
    CHECK(moduleScope->isModule());
    CHECK(moduleScope->getParent() == universe);
    CHECK(moduleScope->getDepth() == 1);
    CHECK(universe->getChildren().size() == 1);

    auto fnScope = moduleScope->createChild(ScopeKind::Function);
    REQUIRE(fnScope != nullptr);
    CHECK(fnScope->isFunction());
    CHECK(fnScope->getDepth() == 2);

    auto blockScope = fnScope->createChild(ScopeKind::Block);
    REQUIRE(blockScope != nullptr);
    CHECK(blockScope->isBlock());
    CHECK(blockScope->getDepth() == 3);

    SECTION("Enclosing scope queries") {
        CHECK(blockScope->getEnclosingFunction() == fnScope);
        CHECK(blockScope->getEnclosingModule() == moduleScope);
        CHECK(blockScope->getEnclosingClass() == nullptr);

        CHECK(fnScope->getEnclosingModule() == moduleScope);
        CHECK(fnScope->getEnclosingFunction() == nullptr); // Checks ancestors above fnScope
        CHECK(moduleScope->getEnclosingModule() == nullptr);
    }

    SECTION("Class and method scopes") {
        auto classScope = moduleScope->createChild(ScopeKind::Class);
        CHECK(classScope->isClass());
        CHECK(classScope->getDepth() == 2);

        auto methodScope = classScope->createChild(ScopeKind::Function);
        CHECK(methodScope->isFunction());
        CHECK(methodScope->getDepth() == 3);

        auto methodBlock = methodScope->createChild(ScopeKind::Block);
        CHECK(methodBlock->isBlock());
        CHECK(methodBlock->getDepth() == 4);

        CHECK(methodBlock->getEnclosingClass() == classScope);
        CHECK(methodBlock->getEnclosingFunction() == methodScope);
        CHECK(methodBlock->getEnclosingModule() == moduleScope);
    }
}

TEST_CASE("Symbol definition and duplicate detection in same scope", "[sema][scope]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    diag::SourceSpan dummySpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 5, 4}};

    SECTION("Successful definition") {
        auto x = std::make_shared<Symbol>("x", SymbolKind::Variable, dummySpan);
        auto res = moduleScope->define(x);
        CHECK(res.success);
        CHECK(res.errorCode == std::nullopt);
        CHECK(moduleScope->size() == 1);
        CHECK(moduleScope->lookupCurrent("x") == x);
    }

    SECTION("Blank identifier '_' does not bind and can be repeated") {
        auto blank1 = std::make_shared<Symbol>("_", SymbolKind::Variable, dummySpan);
        auto blank2 = std::make_shared<Symbol>("_", SymbolKind::Variable, dummySpan);

        auto res1 = moduleScope->define(blank1);
        CHECK(res1.success);
        CHECK(moduleScope->empty());
        CHECK(moduleScope->lookupCurrent("_") == nullptr);

        auto res2 = moduleScope->define(blank2);
        CHECK(res2.success);
        CHECK(moduleScope->empty());
    }

    SECTION("Duplicate declaration in same scope triggers E2004") {
        auto x1 = std::make_shared<Symbol>("total", SymbolKind::Variable, dummySpan);
        auto x2 = std::make_shared<Symbol>("total", SymbolKind::Constant, dummySpan);

        auto res1 = moduleScope->define(x1);
        CHECK(res1.success);

        auto res2 = moduleScope->define(x2);
        CHECK_FALSE(res2.success);
        REQUIRE(res2.errorCode.has_value());
        CHECK(res2.errorCode.value() == diag::ErrorCode::RedeclarationInSameScope);
        CHECK(res2.existingSymbol == x1);
    }

    SECTION("Function overloading in the same scope attaches overload set") {
        auto fn1 = std::make_shared<Symbol>("process", SymbolKind::Function, dummySpan);
        auto fn2 = std::make_shared<Symbol>("process", SymbolKind::Function, dummySpan);

        auto res1 = moduleScope->define(fn1);
        CHECK(res1.success);

        auto res2 = moduleScope->define(fn2);
        CHECK(res2.success);
        CHECK(moduleScope->size() == 1); // Stored under same name

        auto resolved = moduleScope->lookupCurrent("process");
        REQUIRE(resolved == fn1);
        REQUIRE(resolved->hasOverloads());
        CHECK(resolved->getOverloads()[0] == fn2);
    }
}

TEST_CASE("Hierarchical symbol lookup across nested scopes", "[sema][scope]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    auto fnScope = moduleScope->createChild(ScopeKind::Function);
    auto blockScope = fnScope->createChild(ScopeKind::Block);
    diag::SourceSpan dummySpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 5, 4}};

    auto globalVar = std::make_shared<Symbol>("g", SymbolKind::Variable, dummySpan);
    auto paramVar = std::make_shared<Symbol>("p", SymbolKind::Parameter, dummySpan);
    auto localVar = std::make_shared<Symbol>("loc", SymbolKind::Variable, dummySpan);

    REQUIRE(moduleScope->define(globalVar).success);
    REQUIRE(fnScope->define(paramVar).success);
    REQUIRE(blockScope->define(localVar).success);

    SECTION("lookup() resolves at current level first, then bubbles upward") {
        CHECK(blockScope->lookup("loc") == localVar);
        CHECK(blockScope->lookup("p") == paramVar);
        CHECK(blockScope->lookup("g") == globalVar);
        CHECK(blockScope->lookup("nonexistent") == nullptr);

        CHECK(fnScope->lookup("loc") == nullptr);
        CHECK(fnScope->lookup("p") == paramVar);
        CHECK(fnScope->lookup("g") == globalVar);

        CHECK(moduleScope->lookup("loc") == nullptr);
        CHECK(moduleScope->lookup("p") == nullptr);
        CHECK(moduleScope->lookup("g") == globalVar);
    }

    SECTION("lookupCurrent() searches strictly local scope") {
        CHECK(blockScope->lookupCurrent("loc") == localVar);
        CHECK(blockScope->lookupCurrent("p") == nullptr);
        CHECK(blockScope->lookupCurrent("g") == nullptr);

        CHECK(fnScope->lookupCurrent("p") == paramVar);
        CHECK(fnScope->lookupCurrent("g") == nullptr);
    }

    SECTION("lookupParent() searches ancestors only") {
        CHECK(blockScope->lookupParent("loc") == nullptr);
        CHECK(blockScope->lookupParent("p") == paramVar);
        CHECK(blockScope->lookupParent("g") == globalVar);

        CHECK(fnScope->lookupParent("p") == nullptr);
        CHECK(fnScope->lookupParent("g") == globalVar);
    }
}

TEST_CASE("Shadowing candidate lookup and class field exclusion", "[sema][scope]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    diag::SourceSpan dummySpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 5, 4}};

    auto modConst = std::make_shared<Symbol>("TIMEOUT", SymbolKind::Constant, dummySpan);
    REQUIRE(moduleScope->define(modConst).success);

    SECTION("Detects outer module constant as shadowing candidate from function") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto candidate = fnScope->lookupParentForShadowing("TIMEOUT");
        CHECK(candidate == modConst);
    }

    SECTION("Class field is not a shadowing candidate for method parameter or local") {
        auto classScope = moduleScope->createChild(ScopeKind::Class);
        auto field = std::make_shared<Symbol>("width", SymbolKind::Field, dummySpan);
        REQUIRE(classScope->define(field).success);

        auto methodScope = classScope->createChild(ScopeKind::Function);
        auto methodBlock = methodScope->createChild(ScopeKind::Block);

        // Section 741: method parameters and locals can share names with class fields
        CHECK(methodScope->lookupParentForShadowing("width") == nullptr);
        CHECK(methodBlock->lookupParentForShadowing("width") == nullptr);

        // But outer module-level symbol is still detected as shadowing target from within class method
        CHECK(methodBlock->lookupParentForShadowing("TIMEOUT") == modConst);
    }
}
