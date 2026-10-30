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

    bool checkZeroCoercion() {
        typechecker::TypeChecker checker(&emitter);
        return checker.checkZeroCoercion(ast);
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

// =============================================================================
// Task 3.3.2: Strict Zero-Coercion Policy (E3002)
// =============================================================================

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - neg_implicit_widening (E3002)", "[typechecker][E3002]") {
    SECTION("Assigning integer directly to float without cast emits E3002") {
        ParseResult res("let f: float = 42;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));

        // Check diagnostic message and footers
        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E3002");
        REQUIRE(d.message.find("implicit numeric conversion from 'int' to 'float'") != std::string::npos);
        REQUIRE_FALSE(d.labels.empty());
        REQUIRE(d.labels.front().message == "expected 'float', found 'int'");
    }

    SECTION("Float initialized with float literal succeeds") {
        ParseResult res("let f: float = 42.0;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Float initialized with functional cast float(42) succeeds") {
        ParseResult res("let f: float = float(42);");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Float initialized with .toFloat() method call succeeds") {
        ParseResult res("let count: int = 42; let f: float = count.toFloat();");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Float initialized with 'as float' type assertion succeeds") {
        ParseResult res("let f: float = 42 as float;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Constants and Defines (E3002)", "[typechecker][E3002]") {
    SECTION("Constant float initialized with int emits E3002") {
        ParseResult res("const MAX_THRESHOLD: float = 100;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Constant float initialized with float succeeds") {
        ParseResult res("const MAX_THRESHOLD: float = 100.0;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Define float initialized with int emits E3002") {
        ParseResult res("define RATIO: float = 1;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Define float initialized with float succeeds") {
        ParseResult res("define RATIO: float = 1.0;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Reassignment and Compound Assignment (E3002)", "[typechecker][E3002]") {
    SECTION("Assigning int to float variable emits E3002") {
        ParseResult res("fn run(): void { let f: float = 1.0; f = 42; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Assigning float to float variable succeeds") {
        ParseResult res("fn run(): void { let f: float = 1.0; f = 42.0; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Compound assignment float += int emits E3002") {
        ParseResult res("fn run(): void { let f: float = 1.0; f += 42; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Compound assignment float += float succeeds") {
        ParseResult res("fn run(): void { let f: float = 1.0; f += 42.5; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Function Calls and Returns (E3002)", "[typechecker][E3002]") {
    SECTION("Passing int argument to float parameter emits E3002") {
        ParseResult res(
            "fn compute(scale: float): void {}\n"
            "fn run(): void { compute(10); }\n");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Passing float argument to float parameter succeeds") {
        ParseResult res(
            "fn compute(scale: float): void {}\n"
            "fn run(): void { compute(10.0); }\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Passing explicitly converted float(10) argument succeeds") {
        ParseResult res(
            "fn compute(scale: float): void {}\n"
            "fn run(): void { compute(float(10)); }\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Returning int expression in float function emits E3002") {
        ParseResult res("fn getRate(): float { return 42; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Returning float expression in float function succeeds") {
        ParseResult res("fn getRate(): float { return 42.0; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Binary Operators Mixing int and float (E3002)", "[typechecker][E3002]") {
    SECTION("Addition 42 + 3.14 emits E3002") {
        ParseResult res("fn run(): void { let x: float = 42 + 3.14; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Addition 3.14 + 42 emits E3002") {
        ParseResult res("fn run(): void { let x: float = 3.14 + 42; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Multiplication 3.14 * 2 emits E3002") {
        ParseResult res("fn run(): void { let x: float = 3.14 * 2; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Comparison 42 == 3.14 emits E3002") {
        ParseResult res("fn run(): void { let b: bool = (42 == 3.14); }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Relational comparison 42 < 3.14 emits E3002") {
        ParseResult res("fn run(): void { let b: bool = (42 < 3.14); }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Same-type numeric operations succeed") {
        ParseResult res(
            "fn run(): void {\n"
            "    let a: float = 1.5 + 2.5;\n"
            "    let b: int = 10 * 20;\n"
            "    let c: bool = 1.5 < 2.5;\n"
            "    let d: bool = 10 == 20;\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Narrowing float to int (E3002)", "[typechecker][E3002]") {
    SECTION("Assigning float literal directly to int emits E3002") {
        ParseResult res("let i: int = 3.14;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Functional cast int(3.14) succeeds") {
        ParseResult res("let i: int = int(3.14);");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Method call .toInt() succeeds") {
        ParseResult res("let f: float = 3.14; let i: int = f.toInt();");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Task 3.3.2: Strict Zero-Coercion - Class Field Initializers (E3002)", "[typechecker][E3002]") {
    SECTION("Class float field initialized with int emits E3002") {
        ParseResult res("class Account { pub let balance: float = 100; }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Class float field initialized with float succeeds") {
        ParseResult res("class Account { pub let balance: float = 100.0; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

