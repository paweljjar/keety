#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_location.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/shadow_checker.hpp>
#include <keety/sema/symbol.hpp>

using namespace keety;
using namespace keety::sema;

TEST_CASE("Strict No-Shadowing rule enforcement (E2001)", "[sema][shadowing]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    diag::SourceSpan dummySpan1{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 5, 4}};
    diag::SourceSpan dummySpan2{diag::SourceLocation{2, 1, 10}, diag::SourceLocation{2, 5, 14}};

    SECTION("Local variable shadowing outer local in nested block is rejected with E2001") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto outerBlock = fnScope->createChild(ScopeKind::Block);
        auto innerBlock = outerBlock->createChild(ScopeKind::Block);

        auto outerX = std::make_shared<Symbol>("x", SymbolKind::Variable, dummySpan1);
        auto innerX = std::make_shared<Symbol>("x", SymbolKind::Variable, dummySpan2);

        REQUIRE(outerBlock->define(outerX).success);

        auto res = innerBlock->define(innerX);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == outerX);
    }

    SECTION("Local variable shadowing function parameter is rejected with E2001") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);

        auto param = std::make_shared<Symbol>("count", SymbolKind::Parameter, dummySpan1);
        auto local = std::make_shared<Symbol>("count", SymbolKind::Variable, dummySpan2);

        REQUIRE(fnScope->define(param).success);

        auto res = fnBody->define(local);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == param);
    }

    SECTION("Closure parameter shadowing outer function parameter is rejected with E2001") {
        auto outerFn = moduleScope->createChild(ScopeKind::Function);
        auto outerParam = std::make_shared<Symbol>("id", SymbolKind::Parameter, dummySpan1);
        REQUIRE(outerFn->define(outerParam).success);

        auto outerBody = outerFn->createChild(ScopeKind::Block);
        auto closureFn = outerBody->createChild(ScopeKind::Function);
        auto closureParam = std::make_shared<Symbol>("id", SymbolKind::Parameter, dummySpan2);

        auto res = closureFn->define(closureParam);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == outerParam);
    }

    SECTION("Function local variable shadowing module-level variable is rejected with E2001") {
        auto modVar = std::make_shared<Symbol>("config", SymbolKind::Variable, dummySpan1);
        REQUIRE(moduleScope->define(modVar).success);

        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);
        auto localVar = std::make_shared<Symbol>("config", SymbolKind::Variable, dummySpan2);

        auto res = fnBody->define(localVar);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == modVar);
    }

    SECTION("Function local variable shadowing module-level constant is rejected with E2001") {
        auto modConst = std::make_shared<Symbol>("MAX_CAPACITY", SymbolKind::Constant, dummySpan1);
        REQUIRE(moduleScope->define(modConst).success);

        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);
        auto localConst = std::make_shared<Symbol>("MAX_CAPACITY", SymbolKind::Variable, dummySpan2);

        auto res = fnBody->define(localConst);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == modConst);
    }

    SECTION("Function local variable shadowing imported symbol is rejected with E2001") {
        auto importedSym = std::make_shared<Symbol>("sqrt", SymbolKind::ImportedSymbol, dummySpan1);
        REQUIRE(moduleScope->define(importedSym).success);

        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);
        auto local = std::make_shared<Symbol>("sqrt", SymbolKind::Variable, dummySpan2);

        auto res = fnBody->define(local);
        CHECK_FALSE(res.success);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == importedSym);
    }

    SECTION("Loop variable shadowing outer variable is rejected with E2001") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);

        auto outerI = std::make_shared<Symbol>("i", SymbolKind::Variable, dummySpan1);
        REQUIRE(fnBody->define(outerI).success);

        auto loopBlock = fnBody->createChild(ScopeKind::Block);
        auto loopI = std::make_shared<Symbol>("i", SymbolKind::Variable, dummySpan2);

        auto res = loopBlock->define(loopI);
        CHECK_FALSE(res.success);
        CHECK(res.errorCode == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == outerI);
    }

    SECTION("Zonk recovery block parameter shadowing outer variable is rejected with E2001") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);

        auto outerErr = std::make_shared<Symbol>("err", SymbolKind::Variable, dummySpan1);
        REQUIRE(fnBody->define(outerErr).success);

        auto zonkBlock = fnBody->createChild(ScopeKind::Block);
        auto zonkErr = std::make_shared<Symbol>("err", SymbolKind::Constant, dummySpan2);

        auto res = zonkBlock->define(zonkErr);
        CHECK_FALSE(res.success);
        CHECK(res.errorCode == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == outerErr);
    }

    SECTION("Declaring identifier that shadows Universe symbol is rejected with E2001") {
        // Attempting to declare local variable named `int`
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);
        auto shadowInt = std::make_shared<Symbol>("int", SymbolKind::Variable, dummySpan1);

        auto resInt = fnBody->define(shadowInt);
        CHECK_FALSE(resInt.success);
        CHECK(resInt.errorCode == diag::ErrorCode::IdentifierShadowed);

        // Attempting to declare local variable named `true`
        auto shadowTrue = std::make_shared<Symbol>("true", SymbolKind::Variable, dummySpan1);
        auto resTrue = fnBody->define(shadowTrue);
        CHECK_FALSE(resTrue.success);
        CHECK(resTrue.errorCode == diag::ErrorCode::IdentifierShadowed);

        // Attempting to declare local variable named `typeof`
        auto shadowTypeof = std::make_shared<Symbol>("typeof", SymbolKind::Variable, dummySpan1);
        auto resTypeof = fnBody->define(shadowTypeof);
        CHECK_FALSE(resTypeof.success);
        CHECK(resTypeof.errorCode == diag::ErrorCode::IdentifierShadowed);
    }

    SECTION("Blank identifier '_' never shadows and can be repeated") {
        auto fnScope = moduleScope->createChild(ScopeKind::Function);
        auto fnBody = fnScope->createChild(ScopeKind::Block);
        auto innerBlock = fnBody->createChild(ScopeKind::Block);

        auto blank1 = std::make_shared<Symbol>("_", SymbolKind::Variable, dummySpan1);
        auto blank2 = std::make_shared<Symbol>("_", SymbolKind::Variable, dummySpan2);

        CHECK(fnBody->define(blank1).success);
        CHECK(innerBlock->define(blank2).success);
    }
}

TEST_CASE("Class field shadowing exception per Language Spec Section 741", "[sema][shadowing]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    diag::SourceSpan dummySpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 5, 4}};

    // Module-level variable
    auto modVar = std::make_shared<Symbol>("globalValue", SymbolKind::Variable, dummySpan);
    REQUIRE(moduleScope->define(modVar).success);

    // Class definition
    auto classScope = moduleScope->createChild(ScopeKind::Class);
    auto fieldX = std::make_shared<Symbol>("x", SymbolKind::Field, dummySpan);
    REQUIRE(classScope->define(fieldX).success);

    // Method inside class
    auto methodScope = classScope->createChild(ScopeKind::Function);
    auto methodBody = methodScope->createChild(ScopeKind::Block);

    SECTION("Method parameter can share name with class field (this.x)") {
        auto paramX = std::make_shared<Symbol>("x", SymbolKind::Parameter, dummySpan);
        auto res = methodScope->define(paramX);
        CHECK(res.success);
        CHECK(methodScope->lookupCurrent("x") == paramX);
    }

    SECTION("Method local variable can share name with class field (this.x)") {
        auto localX = std::make_shared<Symbol>("x", SymbolKind::Variable, dummySpan);
        auto res = methodBody->define(localX);
        CHECK(res.success);
        CHECK(methodBody->lookupCurrent("x") == localX);
    }

    SECTION("Method parameter cannot shadow outer module variable") {
        auto paramGlobal = std::make_shared<Symbol>("globalValue", SymbolKind::Parameter, dummySpan);
        auto res = methodScope->define(paramGlobal);
        CHECK_FALSE(res.success);
        CHECK(res.errorCode == diag::ErrorCode::IdentifierShadowed);
        CHECK(res.existingSymbol == modVar);
    }
}

TEST_CASE("ShadowChecker diagnostic generation and emission", "[sema][shadowing]") {
    diag::SourceManager sm;
    auto fileId = sm.addBuffer("test.keet", "let x: int = 1;\nlet x: int = 2;\n");
    diag::DiagnosticEmitter emitter(sm);

    diag::SourceSpan span1{diag::SourceLocation{fileId, 4, 1, 5}, diag::SourceLocation{fileId, 5, 1, 6}};
    diag::SourceSpan span2{diag::SourceLocation{fileId, 20, 2, 5}, diag::SourceLocation{fileId, 21, 2, 6}};

    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    auto fnScope = moduleScope->createChild(ScopeKind::Function);

    auto outer = std::make_shared<Symbol>("x", SymbolKind::Variable, span1);
    auto inner = std::make_shared<Symbol>("x", SymbolKind::Variable, span2);

    REQUIRE(moduleScope->define(outer).success);

    auto res = ShadowChecker::defineWithDiagnostics(*fnScope, inner, &emitter);
    CHECK_FALSE(res.success);
    CHECK(emitter.hasErrors());
    CHECK(emitter.getErrorCount() == 1);

    const auto& diags = emitter.getDiagnostics();
    REQUIRE(diags.size() == 1);
    CHECK(diags[0].code == "E2001");
    CHECK(diags[0].message.find("shadows outer declaration") != std::string::npos);

    auto formatted = emitter.format(diags[0]);
    CHECK(formatted.find("error[E2001]") != std::string::npos);
    CHECK(formatted.find("re-declaration of 'x' shadows outer binding here") != std::string::npos);
    CHECK(formatted.find("previous declaration of 'x' is here") != std::string::npos);
    CHECK(formatted.find("KeetScript strictly forbids shadowing") != std::string::npos);
}
