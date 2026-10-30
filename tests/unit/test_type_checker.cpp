#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/typechecker/type_checker.hpp>

using namespace keety;

namespace {

struct ParseResult {
    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter;
    parser::ModuleAST ast;

    ParseResult(std::string_view source) : emitter(sm) {
        auto fileId = sm.addBuffer("test.keet", std::string(source));
        lexer::Lexer lexer(fileId, source, &emitter);
        parser::Parser parser(lexer, &emitter);
        ast = parser.parseModule("test");
    }

    [[nodiscard]] bool hasError(diag::ErrorCode code) const {
        std::string_view codeStr = diag::getErrorCodeString(code);
        for (const auto& d : emitter.getDiagnostics()) {
            if (d.code == codeStr) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

TEST_CASE("Task 3.3.1: Explicit Type Annotations - Variable declarations (E3001)", "[typechecker][E3001]") {
    SECTION("Variable declaration without type annotation emits E3001") {
        ParseResult res("let x = 10;");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE_FALSE(checker.checkExplicitTypeAnnotations(res.ast));
        REQUIRE(checker.hasErrors());
    }

    SECTION("Variable declaration with explicit type annotation succeeds") {
        ParseResult res("let x: int = 10;");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
        REQUIRE_FALSE(checker.hasErrors());
    }
}

TEST_CASE("Task 3.3.1: Explicit Type Annotations - Constant and Define declarations (E3001)", "[typechecker][E3001]") {
    SECTION("Const declaration without type annotation emits E3001") {
        ParseResult res("const PI = 3.14;");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Const declaration with type annotation succeeds") {
        ParseResult res("const PI: float = 3.14;");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }

    SECTION("Define declaration without type annotation emits E3001") {
        ParseResult res("define MAX_RETRIES = 5;");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Define declaration with type annotation succeeds") {
        ParseResult res("define MAX_RETRIES: int = 5;");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }
}

TEST_CASE("Task 3.3.1: Explicit Type Annotations - Function declarations and parameters (E3001)", "[typechecker][E3001]") {
    SECTION("Function parameter without type annotation emits E3001") {
        ParseResult res("fn add(a, b: int): int { return a + b; }");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Function without return type annotation emits E3001") {
        ParseResult res("fn greet(name: string) { return; }");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Fully-annotated function succeeds") {
        ParseResult res("fn add(a: int, b: int): int { return a + b; }");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
        REQUIRE_FALSE(checker.hasErrors());
    }
}

TEST_CASE("Task 3.3.1: Explicit Type Annotations - Closures and Function Literals (E3001)", "[typechecker][E3001]") {
    SECTION("Closure parameter without type annotation emits E3001") {
        ParseResult resUntypedParam("let f: Handler = fn(x): int { return x; };");
        REQUIRE(resUntypedParam.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Closure without return type annotation emits E3001") {
        ParseResult res("let f: Handler = fn(x: int) { return x; };");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Fully typed closure succeeds") {
        ParseResult res("let f: Handler = fn(x: int): int { return x + 1; };");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }
}

TEST_CASE("Task 3.3.1: Explicit Type Annotations - Class and ErrType fields (E3001)", "[typechecker][E3001]") {
    SECTION("Class field without type annotation emits E3001") {
        ParseResult res("class User { pub let name; }");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("Class field with type annotation succeeds") {
        ParseResult res("class User { pub let name: string; }");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }

    SECTION("ErrType field without type annotation emits E3001") {
        ParseResult res("errtype NetworkError { pub let code; };");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("ErrType field with type annotation succeeds") {
        ParseResult res("errtype NetworkError { pub let code: int; };");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }
}

TEST_CASE("Task 3.3.1: Explicit Type Annotations - For-in loop variable bindings (E3001)", "[typechecker][E3001]") {
    SECTION("For-in loop variable without type annotation emits E3001") {
        ParseResult res("fn run(): void { let arr: array<int> = [1, 2]; for (let x in arr) { } }");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("For-in key-value bindings without type annotation emit E3001") {
        ParseResult res("fn run(): void { let m: map<string, int> = {}; for (let k: string, v in m) { } }");
        REQUIRE(res.hasError(diag::ErrorCode::MissingTypeAnnotation));
    }

    SECTION("For-in with explicit type annotations succeeds") {
        ParseResult res("fn run(): void { let arr: array<int> = [1, 2]; for (let x: int in arr) { } }");
        REQUIRE_FALSE(res.emitter.hasErrors());

        typechecker::TypeChecker checker(&res.emitter);
        REQUIRE(checker.checkExplicitTypeAnnotations(res.ast));
    }
}

TEST_CASE("Task 3.3.1: TypeChecker standalone AST check emits E3001", "[typechecker][E3001]") {
    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm);
    typechecker::TypeChecker checker(&emitter);

    // Create an unannotated VarDecl directly in the AST
    diag::SourceSpan span{diag::SourceLocation{1, 1, 0}, diag::SourceLocation{1, 10, 9}};
    auto varDecl = std::make_unique<ast::VarDecl>(span, "untypedVar", nullptr, nullptr, false);

    bool result = checker.checkExplicitTypeAnnotations(*varDecl);
    REQUIRE_FALSE(result);
    REQUIRE(checker.hasErrors());
    REQUIRE(emitter.hasErrors());
    REQUIRE(emitter.getDiagnostics().front().code == diag::getErrorCodeString(diag::ErrorCode::MissingTypeAnnotation));
}
