#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_location.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/sema/const_view_checker.hpp>
#include <keety/sema/symbol.hpp>

using namespace keety;
using namespace keety::sema;

TEST_CASE("Asymmetric const-view semantics for export let", "[sema][const_view]") {
    diag::SourceSpan defSpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 30, 29}};
    diag::SourceSpan impSpan{diag::SourceLocation{2, 1, 0}, diag::SourceLocation{2, 35, 34}};

    // 1. Defining module: export let activeConnections: int = 0;
    auto definingVar = std::make_shared<Symbol>("activeConnections", SymbolKind::Variable, defSpan);
    definingVar->setExported(true);
    definingVar->setMutable(true);
    definingVar->setConst(false);

    SECTION("Defining module has full mutable access") {
        CHECK(definingVar->isExported());
        CHECK(definingVar->isMutable());
        CHECK_FALSE(definingVar->isConst());
        CHECK_FALSE(definingVar->isConstView());
        CHECK(definingVar->canMutate());
        CHECK(definingVar->isAssignable());

        auto res = ConstViewChecker::validateMutation(*definingVar, defSpan, MutationKind::Assignment);
        CHECK(res.valid);
        CHECK(res.errorCode == std::nullopt);
    }

    SECTION("Importing module receives immutable const-view") {
        auto imported = ConstViewChecker::importExportedSymbol(definingVar, impSpan);
        REQUIRE(imported != nullptr);
        CHECK(imported->getName() == "activeConnections");
        CHECK(imported->isConstView());
        CHECK(imported->isConst());
        CHECK_FALSE(imported->isMutable());
        CHECK_FALSE(imported->canMutate());
        CHECK_FALSE(imported->isAssignable());
        CHECK(imported->getOriginalExportedSymbol() == definingVar);
    }
}

TEST_CASE("Rejection of mutations on imported export let with E2002", "[sema][const_view]") {
    diag::SourceSpan defSpan{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 30, 29}};
    diag::SourceSpan impSpan{diag::SourceLocation{2, 1, 0}, diag::SourceLocation{2, 35, 34}};
    diag::SourceSpan writeSpan{diag::SourceLocation{2, 10, 100}, diag::SourceLocation{2, 25, 115}};

    auto definingVar = std::make_shared<Symbol>("requestCount", SymbolKind::Variable, defSpan);
    definingVar->setExported(true);
    definingVar->setMutable(true);

    auto importedConstView = ConstViewChecker::importExportedSymbol(definingVar, impSpan);
    REQUIRE(importedConstView != nullptr);

    SECTION("Direct assignment is rejected with E2002") {
        auto res = ConstViewChecker::validateMutation(*importedConstView, writeSpan, MutationKind::Assignment);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::ExportConstWrite);
    }

    SECTION("Compound assignment is rejected with E2002") {
        auto res = ConstViewChecker::validateMutation(*importedConstView, writeSpan, MutationKind::CompoundAssignment);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::ExportConstWrite);
    }

    SECTION("Increment/decrement is rejected with E2002") {
        auto res = ConstViewChecker::validateMutation(*importedConstView, writeSpan, MutationKind::IncrementDecrement);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::ExportConstWrite);
    }

    SECTION("Mutable borrow ('mut') is rejected with E2002") {
        auto res = ConstViewChecker::validateMutation(*importedConstView, writeSpan, MutationKind::MutableBorrow);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::ExportConstWrite);
    }

    SECTION("Mutating method call is rejected with E2002") {
        auto res = ConstViewChecker::validateMutation(*importedConstView, writeSpan, MutationKind::MutatingMethodCall);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::ExportConstWrite);
    }
}

TEST_CASE("Contrast with regular constant write (E2005 vs E2002)", "[sema][const_view]") {
    diag::SourceSpan span{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 10, 9}};
    diag::SourceSpan writeSpan{diag::SourceLocation{1, 15, 14}, diag::SourceLocation{1, 25, 24}};

    SECTION("Standard local or module const triggers E2005, not E2002") {
        auto constSym = std::make_shared<Symbol>("MAX_USERS", SymbolKind::Constant, span);
        constSym->setConst(true);
        constSym->setMutable(false);

        auto res = ConstViewChecker::validateMutation(*constSym, writeSpan, MutationKind::Assignment);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::CannotAssignToConstant);
        CHECK(res.errorCode.value() != diag::ErrorCode::ExportConstWrite);
    }

    SECTION("Standard define triggers E2005, not E2002") {
        auto defSym = std::make_shared<Symbol>("TIMEOUT", SymbolKind::Define, span);
        defSym->setConst(true);
        defSym->setMutable(false);

        auto res = ConstViewChecker::validateMutation(*defSym, writeSpan, MutationKind::Assignment);
        CHECK_FALSE(res.valid);
        REQUIRE(res.errorCode.has_value());
        CHECK(res.errorCode.value() == diag::ErrorCode::CannotAssignToConstant);
    }

    SECTION("Local mutable variable passes validation without error") {
        auto local = std::make_shared<Symbol>("temp", SymbolKind::Variable, span);
        local->setMutable(true);
        local->setConst(false);

        auto res = ConstViewChecker::validateMutation(*local, writeSpan, MutationKind::Assignment);
        CHECK(res.valid);
        CHECK(res.errorCode == std::nullopt);
    }
}

TEST_CASE("ConstViewChecker diagnostic rendering with DiagnosticEmitter", "[sema][const_view]") {
    diag::SourceManager sm;
    auto serverFile = sm.addBuffer("server.keet", "export let activeConnections: int = 0;\n");
    auto clientFile = sm.addBuffer("client.keet", "import \"./server\" as server;\nserver.activeConnections += 1;\n");

    diag::SourceSpan defSpan{diag::SourceLocation{serverFile, 0, 1, 1}, diag::SourceLocation{serverFile, 38, 1, 39}};
    diag::SourceSpan impSpan{diag::SourceLocation{clientFile, 0, 1, 1}, diag::SourceLocation{clientFile, 29, 1, 30}};
    diag::SourceSpan writeSpan{diag::SourceLocation{clientFile, 30, 2, 1}, diag::SourceLocation{clientFile, 57, 2, 28}};

    auto definingVar = std::make_shared<Symbol>("activeConnections", SymbolKind::Variable, defSpan);
    definingVar->setExported(true);
    definingVar->setMutable(true);

    auto importedConstView = ConstViewChecker::importExportedSymbol(definingVar, impSpan);

    diag::DiagnosticEmitter emitter(sm);
    auto res = ConstViewChecker::validateMutation(
        *importedConstView, writeSpan, MutationKind::CompoundAssignment, &emitter);

    CHECK_FALSE(res.valid);
    CHECK(emitter.hasErrors());
    CHECK(emitter.getErrorCount() == 1);

    const auto& diags = emitter.getDiagnostics();
    REQUIRE(diags.size() == 1);
    CHECK(diags[0].code == "E2002");
    CHECK(diags[0].message.find("cannot modify imported 'export let' variable 'activeConnections'") != std::string::npos);

    auto formatted = emitter.format(diags[0]);
    CHECK(formatted.find("error[E2002]") != std::string::npos);
    CHECK(formatted.find("cannot modify 'export let' variable across module boundaries via compound assignment") != std::string::npos);
    CHECK(formatted.find("variable 'activeConnections' is declared as 'export let' here") != std::string::npos);
    CHECK(formatted.find("asymmetric const-view semantics") != std::string::npos);
    CHECK(formatted.find("export an explicit mutator function") != std::string::npos);
}
