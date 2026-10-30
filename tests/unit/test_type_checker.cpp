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

TEST_CASE("Explicit Type Annotations - Variable declarations (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("Explicit Type Annotations - Constant and Define declarations (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("Explicit Type Annotations - Function declarations and parameters (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("Explicit Type Annotations - Closures and Function Literals (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("Explicit Type Annotations - Class and ErrType fields (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("Explicit Type Annotations - For-in loop variable bindings (E3001)", "[typechecker][E3001]") {
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

TEST_CASE("TypeChecker standalone AST check emits E3001", "[typechecker][E3001]") {
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
// Strict Zero-Coercion Policy (E3002)
// =============================================================================

TEST_CASE("Strict Zero-Coercion - neg_implicit_widening (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Constants and Defines (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Reassignment and Compound Assignment (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Function Calls and Returns (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Binary Operators Mixing int and float (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Narrowing float to int (E3002)", "[typechecker][E3002]") {
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

TEST_CASE("Strict Zero-Coercion - Class Field Initializers (E3002)", "[typechecker][E3002]") {
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

// =============================================================================
// Contextual Typing of Empty Collection Literals (E3009 / E3003)
// =============================================================================

TEST_CASE("Contextual Typing - Empty Array Literals '[]'", "[typechecker][contextual]") {
    SECTION("Variable declaration context: let a: array<int> = [] succeeds") {
        ParseResult res("let a: array<int> = [];");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Constant context: const a: array<string> = [] succeeds") {
        ParseResult res("const a: array<string> = [];");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Compile-time define context: define EMPTY: array<bool> = [] succeeds") {
        ParseResult res("define EMPTY: array<bool> = [];");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Reassignment context: a = [] succeeds") {
        ParseResult res(
            "fn run(): void {\n"
            "    let a: array<int> = [1, 2];\n"
            "    a = [];\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Function call argument context: clear([]) succeeds") {
        ParseResult res(
            "fn clear(items: array<int>): void {}\n"
            "fn run(): void { clear([]); }\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Function return statement context: return [] succeeds") {
        ParseResult res(
            "fn getItems(): array<float> {\n"
            "    return [];\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Class field default initializer context succeeds") {
        ParseResult res("class Cache { pub let items: array<string> = []; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Contextual Typing - Empty Map Literals '{}'", "[typechecker][contextual]") {
    SECTION("Variable declaration context: let m: map<string, int> = {} succeeds") {
        ParseResult res("let m: map<string, int> = {};");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Constant context: const m: map<string, string> = {} succeeds") {
        ParseResult res("const m: map<string, string> = {};");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Compile-time define context: define EMPTY_MAP: map<int, int> = {} succeeds") {
        ParseResult res("define EMPTY_MAP: map<int, int> = {};");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Reassignment context: m = {} succeeds") {
        ParseResult res(
            "fn run(): void {\n"
            "    let m: map<string, int> = {\"x\": 1};\n"
            "    m = {};\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Function call argument context: reset({}) succeeds") {
        ParseResult res(
            "fn reset(config: map<string, string>): void {}\n"
            "fn run(): void { reset({}); }\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Function return statement context: return {} succeeds") {
        ParseResult res(
            "fn getScores(): map<string, int> {\n"
            "    return {};\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Class field default initializer context succeeds") {
        ParseResult res("class Config { pub let settings: map<string, string> = {}; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Contextual Typing - Nested Empty Collections", "[typechecker][contextual]") {
    SECTION("Nested empty arrays in array<array<int>> succeed") {
        ParseResult res("let matrix: array<array<int>> = [[], [1, 2], []];");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Nested empty array in map<string, array<int>> succeeds") {
        ParseResult res("let table: map<string, array<int>> = {\"primes\": [2, 3], \"empty\": []};");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Nested empty map in map<string, map<string, int>> succeeds") {
        ParseResult res("let nested: map<string, map<string, int>> = {\"group\": {}};");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Contextual Typing - Ternary and Zonk Fallback Collections", "[typechecker][contextual]") {
    SECTION("Empty array in ternary conditional succeeds") {
        ParseResult res(
            "fn run(flag: bool): void {\n"
            "    let a: array<int> = flag ? [1, 2] : [];\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Empty array in zonk fallback succeeds") {
        ParseResult res(
            "fn fetch(): array<int> { return []; }\n"
            "fn run(): void {\n"
            "    let a: array<int> = fetch() zonk [];\n"
            "}\n");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Contextual Typing - Untyped Empty Collections Emit E3009", "[typechecker][E3009]") {
    SECTION("typeof([]) without contextual type emits E3009") {
        ParseResult res("fn run(): void { let s: string = typeof([]); }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::CannotInferCollectionType));

        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E3009");
        REQUIRE(d.message.find("cannot infer type of empty array literal '[]'") != std::string::npos);
    }

    SECTION("typeof({}) without contextual type emits E3009") {
        ParseResult res("fn run(): void { let s: string = typeof({}); }");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::CannotInferCollectionType));

        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E3009");
        REQUIRE(d.message.find("cannot infer type of empty map literal '{}'") != std::string::npos);
    }
}

TEST_CASE("Contextual Typing - Incompatible Collection Types Emit E3003", "[typechecker][E3003]") {
    SECTION("Empty array assigned to scalar int emits E3003") {
        ParseResult res("let x: int = [];");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));

        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E3003");
        REQUIRE(d.message.find("type mismatch: expected 'int'") != std::string::npos);
    }

    SECTION("Empty map assigned to scalar string emits E3003") {
        ParseResult res("let s: string = {};");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Empty map assigned to array<int> emits E3003") {
        ParseResult res("let a: array<int> = {};");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Empty array assigned to map<string, int> emits E3003") {
        ParseResult res("let m: map<string, int> = [];");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }
}

TEST_CASE("Contextual Typing - Non-Empty Collection Elements Checked", "[typechecker]") {
    SECTION("Array elements checked for zero-coercion (E3002)") {
        ParseResult res("let a: array<float> = [42];");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Map values checked for zero-coercion (E3002)") {
        ParseResult res("let m: map<string, float> = {\"k\": 42};");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ImplicitTypeWidening));
    }

    SECTION("Array elements checked for type mismatch (E3003)") {
        ParseResult res("let a: array<int> = [\"not_an_int\"];");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }
}

// =============================================================================
// Canonical Union Algebra Normalization
// =============================================================================

TEST_CASE("Union Algebra - Flattening (Associativity)", "[typechecker][union]") {
    SECTION("Direct API: A | (B | C) flattens to A | B | C") {
        auto tA = typechecker::Type::intType();
        auto tB = typechecker::Type::stringType();
        auto tC = typechecker::Type::boolType();

        auto innerUnion = typechecker::Type::unionOf({tB, tC});
        auto outerUnion = typechecker::Type::unionOf({tA, innerUnion});

        REQUIRE(outerUnion->isUnion());
        const auto& u = static_cast<const typechecker::UnionType&>(*outerUnion);
        REQUIRE(u.getVariants().size() == 3);

        // Flattened directly
        auto directUnion = typechecker::Type::unionOf({tA, tB, tC});
        REQUIRE(outerUnion->equals(*directUnion));
    }

    SECTION("Direct API: ((A | B) | (C | D)) flattens recursively") {
        auto tA = typechecker::Type::intType();
        auto tB = typechecker::Type::stringType();
        auto tC = typechecker::Type::boolType();
        auto tD = typechecker::Type::charType();

        auto u1 = typechecker::Type::unionOf({tA, tB});
        auto u2 = typechecker::Type::unionOf({tC, tD});
        auto total = typechecker::Type::unionOf({u1, u2});

        REQUIRE(total->isUnion());
        const auto& u = static_cast<const typechecker::UnionType&>(*total);
        REQUIRE(u.getVariants().size() == 4);
    }

    SECTION("Source AST: Parenthesized union in declaration flattens") {
        ParseResult res("let x: int | (string | bool) = 1;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Union Algebra - Commutativity and Deduplication", "[typechecker][union]") {
    SECTION("Direct API: int | string is equal to string | int") {
        auto u1 = typechecker::Type::unionOf({typechecker::Type::intType(), typechecker::Type::stringType()});
        auto u2 = typechecker::Type::unionOf({typechecker::Type::stringType(), typechecker::Type::intType()});

        REQUIRE(u1->equals(*u2));
        REQUIRE(u2->equals(*u1));
        REQUIRE(u1->toString() == u2->toString());
    }

    SECTION("Direct API: int | string | int deduplicates to int | string") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::intType(),
            typechecker::Type::stringType(),
            typechecker::Type::intType()
        });

        REQUIRE(u->isUnion());
        const auto& unionT = static_cast<const typechecker::UnionType&>(*u);
        REQUIRE(unionT.getVariants().size() == 2);

        auto expected = typechecker::Type::unionOf({typechecker::Type::intType(), typechecker::Type::stringType()});
        REQUIRE(u->equals(*expected));
    }

    SECTION("Direct API: complex commutativity with null") {
        auto u1 = typechecker::Type::unionOf({
            typechecker::Type::intType(),
            typechecker::Type::stringType(),
            typechecker::Type::nullType()
        });
        auto u2 = typechecker::Type::unionOf({
            typechecker::Type::nullType(),
            typechecker::Type::intType(),
            typechecker::Type::stringType()
        });

        REQUIRE(u1->equals(*u2));
        REQUIRE(u1->toString() == u2->toString());
    }

    SECTION("Source AST: Duplicate variants in type annotation parse and succeed") {
        ParseResult res("let x: int | string | int = 10;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Union Algebra - Subtype and Literal Absorption", "[typechecker][union]") {
    SECTION("Direct API: 200 | 404 | int normalizes canonically to int") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalInt(200),
            typechecker::Type::literalInt(404),
            typechecker::Type::intType()
        });

        // Subtype absorption reduces the union to int
        REQUIRE(u->isInt());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::intType()));
        REQUIRE(u->toString() == "int");
    }

    SECTION("Direct API: 'a' | 'b' | char normalizes to char") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalChar('a'),
            typechecker::Type::literalChar('b'),
            typechecker::Type::charType()
        });

        REQUIRE(u->isChar());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::charType()));
    }

    SECTION("Direct API: \"read\" | \"write\" | string normalizes to string") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalString("read"),
            typechecker::Type::literalString("write"),
            typechecker::Type::stringType()
        });

        REQUIRE(u->isString());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::stringType()));
    }

    SECTION("Direct API: true | false absorbs into bool") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalBool(true),
            typechecker::Type::literalBool(false)
        });

        REQUIRE(u->isBool());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::boolType()));
    }

    SECTION("Direct API: true | bool absorbs into bool") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalBool(true),
            typechecker::Type::boolType()
        });

        REQUIRE(u->isBool());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::boolType()));
    }

    SECTION("Direct API: 3.14 | float absorbs into float") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::literalFloat(3.14),
            typechecker::Type::floatType()
        });

        REQUIRE(u->isFloat());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::floatType()));
    }

    SECTION("Source AST: 200 | 404 | int annotation absorbs into int") {
        ParseResult res("let statusCode: 200 | 404 | int = 200;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Source AST: true | false annotation absorbs into bool") {
        ParseResult res("let flag: true | false = true;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Union Algebra - Single-Variant Unwrapping", "[typechecker][union]") {
    SECTION("Direct API: int | int unwraps to int") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::intType(),
            typechecker::Type::intType()
        });

        REQUIRE(u->isInt());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::intType()));
    }

    SECTION("Direct API: union of 1 element unwraps to that element") {
        auto u = typechecker::Type::unionOf({typechecker::Type::stringType()});

        REQUIRE(u->isString());
        REQUIRE_FALSE(u->isUnion());
        REQUIRE(u->equals(*typechecker::Type::stringType()));
    }

    SECTION("Direct API: nested redundant unions unwrap completely") {
        auto u = typechecker::Type::unionOf({
            typechecker::Type::unionOf({typechecker::Type::intType()}),
            typechecker::Type::unionOf({typechecker::Type::intType()})
        });

        REQUIRE(u->isInt());
        REQUIRE_FALSE(u->isUnion());
    }
}

TEST_CASE("Union and Literal Types - Positive Conformance", "[typechecker][union][literal]") {
    SECTION("Integer literal union: 200 | 404 | 500") {
        ParseResult res("let httpStatus: 200 | 404 | 500 = 200;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Signed integer literal union: -1 | 0 | 1") {
        ParseResult res(
            "let d1: -1 | 0 | 1 = -1;\n"
            "let d2: -1 | 0 | 1 = 0;\n"
            "let d3: -1 | 0 | 1 = 1;\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Char literal union: 'y' | 'n'") {
        ParseResult res("let confirmation: 'y' | 'n' = 'y';");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("String literal union: \"read\" | \"write\"") {
        ParseResult res("let mode: \"read\" | \"write\" = \"read\";");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Mixed literal and primitive union: \"read\" | \"write\" | bool") {
        ParseResult res("let mode: \"read\" | \"write\" | bool = false;");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Assigning literal union to base primitive succeeds") {
        ParseResult res(
            "let httpStatus: 200 | 404 = 200;\n"
            "let code: int = httpStatus;\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Assigning narrower union to wider union succeeds") {
        ParseResult res(
            "let x: int | string = 42;\n"
            "let y: int | string | null = x;\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Union and Literal Types - Negative Rejection (E3003)", "[typechecker][union][literal][E3003]") {
    SECTION("Integer value not in literal union emits E3003") {
        ParseResult res("let httpStatus: 200 | 404 = 500;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Unconstrained int assigned to literal union emits E3003") {
        ParseResult res(
            "let code: int = 200;\n"
            "let httpStatus: 200 | 404 = code;\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Char value not in literal union emits E3003") {
        ParseResult res("let confirmation: 'y' | 'n' = 'x';");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("String value not in literal union emits E3003") {
        ParseResult res("let mode: \"read\" | \"write\" = \"execute\";");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Signed integer value out of range emits E3003") {
        ParseResult res("let direction: -1 | 0 | 1 = 2;");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Wider union assigned to narrower union emits E3003") {
        ParseResult res(
            "let y: int | string | null = null;\n"
            "let x: int | string = y;\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }
}

// =============================================================================
// Phase 3.4: Control-Flow Analysis (CFA) & Type Narrowing
// =============================================================================

TEST_CASE("Flow-Based Type Narrowing - typeof(x) == \"T\"", "[cfa][narrowing]") {
    SECTION("Variable narrowed to int inside then branch") {
        ParseResult res(
            "fn test(x: int | string): void {\n"
            "    if (typeof(x) == \"int\") {\n"
            "        let y: int = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Variable narrowed to int cannot be assigned to string in then branch (E3003)") {
        ParseResult res(
            "fn test(x: int | string): void {\n"
            "    if (typeof(x) == \"int\") {\n"
            "        let s: string = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Variable filtered in else branch of typeof check") {
        ParseResult res(
            "fn test(x: int | string): void {\n"
            "    if (typeof(x) == \"int\") {\n"
            "        let i: int = x;\n"
            "    } else {\n"
            "        let s: string = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Conformance Test 3.1: formatValue with if-elif-else narrowing") {
        ParseResult res(
            "fn formatValue(v: int | string | null): string {\n"
            "    if (typeof(v) == \"int\") {\n"
            "        let i: int = v;\n"
            "        return \"int\";\n"
            "    } elif (typeof(v) == \"string\") {\n"
            "        let s: string = v;\n"
            "        return \"str\";\n"
            "    } else {\n"
            "        let n: null = v;\n"
            "        return \"empty\";\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Symmetric check: \"string\" == typeof(x)") {
        ParseResult res(
            "fn test(x: int | string): void {\n"
            "    if (\"string\" == typeof(x)) {\n"
            "        let s: string = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Inequality typeof check: typeof(x) != \"int\" narrows then branch to string") {
        ParseResult res(
            "fn test(x: int | string): void {\n"
            "    if (typeof(x) != \"int\") {\n"
            "        let s: string = x;\n"
            "    } else {\n"
            "        let i: int = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Flow-Based Type Narrowing - Non-Null Checks (x != null)", "[cfa][narrowing]") {
    SECTION("Nullable variable narrowed to non-null inside if (x != null)") {
        ParseResult res(
            "fn test(x: int | null): void {\n"
            "    if (x != null) {\n"
            "        let y: int = x;\n"
            "        let sum: int = x + 10;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Else branch of if (x != null) narrows to null") {
        ParseResult res(
            "fn test(x: int | null): void {\n"
            "    if (x != null) {\n"
            "        let y: int = x;\n"
            "    } else {\n"
            "        let n: null = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Variable narrowed to null in else branch cannot be used as int (E3003)") {
        ParseResult res(
            "fn test(x: int | null): void {\n"
            "    if (x != null) {\n"
            "        let y: int = x;\n"
            "    } else {\n"
            "        let y: int = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Checking if (x == null) narrows then to null and else to non-null") {
        ParseResult res(
            "fn test(x: string | null): void {\n"
            "    if (x == null) {\n"
            "        let n: null = x;\n"
            "    } else {\n"
            "        let s: string = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Symmetric null check: null != x") {
        ParseResult res(
            "fn test(x: float | null): void {\n"
            "    if (null != x) {\n"
            "        let f: float = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Multi-union non-null check: int | string | null narrows to int | string") {
        ParseResult res(
            "fn test(x: int | string | null): void {\n"
            "    if (x != null) {\n"
            "        let u: int | string = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Flow-Based Type Narrowing - Compound Conditions & Ternaries", "[cfa][narrowing]") {
    SECTION("Compound condition with && narrows multiple constraints") {
        ParseResult res(
            "fn test(x: int | string | null): void {\n"
            "    if (x != null && typeof(x) == \"int\") {\n"
            "        let i: int = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Ternary conditional narrowing") {
        ParseResult res(
            "fn test(x: string | null): string {\n"
            "    let s: string = (x != null) ? x : \"default\";\n"
            "    return s;\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("While loop condition narrows inside body") {
        ParseResult res(
            "fn test(x: int | null): void {\n"
            "    while (x != null) {\n"
            "        let i: int = x;\n"
            "        break;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Mutation Watcher - Direct Reassignment Invalidation", "[cfa][narrowing][invalidation]") {
    SECTION("Conformance Test 3.2: Reassignment inside guarded block compiles cleanly") {
        ParseResult res(
            "let item: int | string = 100;\n"
            "if (typeof(item) == \"int\") {\n"
            "    let doubled: int = item * 2;\n"
            "    item = \"reassigned\";\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Using item as narrowed type after reassignment emits E3003") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        let doubled: int = item * 2;\n"
            "        item = \"reassigned\";\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Reassigning same-type value still invalidates narrowed type") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        item = 200;\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Subsequent read accepts declared union type after reassignment") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        let doubled: int = item * 2;\n"
            "        item = \"reassigned\";\n"
            "        let u: int | string = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Nullable variable reassignment invalidates non-null narrowing") {
        ParseResult res(
            "fn test(): void {\n"
            "    let x: int | null = 42;\n"
            "    if (x != null) {\n"
            "        let a: int = x;\n"
            "        x = null;\n"
            "        let b: int = x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Re-narrowing after reassignment invalidation succeeds") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        item = \"reassigned\";\n"
            "        if (typeof(item) == \"string\") {\n"
            "            let s: string = item;\n"
            "        }\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Mutation Watcher - Compound Assignment & Unary Mutation", "[cfa][narrowing][invalidation]") {
    SECTION("Compound assignment item += 5 invalidates narrowing") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        item += 5;\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Unary prefix increment ++item invalidates narrowing") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        ++item;\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Unary prefix decrement --item invalidates narrowing") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        --item;\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }
}

TEST_CASE("Mutation Watcher - Passing to mut Parameters", "[cfa][narrowing][invalidation]") {
    SECTION("Passing narrowed variable to mut parameter invalidates narrowing") {
        ParseResult res(
            "fn modify(mut x: int | string): void {}\n"
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        modify(mut item);\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Passing narrowed variable to mut parameter without assumption of int succeeds") {
        ParseResult res(
            "fn modify(mut x: int | string): void {}\n"
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        modify(mut item);\n"
            "        let u: int | string = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Mutation Watcher - Closure Mutation & Enclosing Scopes", "[cfa][narrowing][invalidation]") {
    SECTION("Mutation in nested inner block invalidates narrowing in enclosing scope") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        {\n"
            "            item = \"changed_in_inner\";\n"
            "        }\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Mutation of captured variable inside closure invalidates enclosing scope") {
        ParseResult res(
            "fn test(): void {\n"
            "    let item: int | string = 100;\n"
            "    if (typeof(item) == \"int\") {\n"
            "        let cl = fn(): void {\n"
            "            item = \"closure_mutated\";\n"
            "        };\n"
            "        let i: int = item;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
    }
}

TEST_CASE("Exhaustiveness Analysis - Missing Return in Non-Void Functions (E5001)", "[cfa][exhaustiveness][E5001]") {
    SECTION("Empty body in non-void function emits E5001") {
        ParseResult res("fn compute(): int {}");
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));

        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E5001");
        REQUIRE(d.message.find("missing return statement in non-void function 'compute'") != std::string::npos);
    }

    SECTION("Partial return inside if without else emits E5001") {
        ParseResult res(
            "fn abs(x: int): int {\n"
            "    if (x > 0) {\n"
            "        return x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("Non-terminating while loop emits E5001") {
        ParseResult res(
            "fn loop(x: int): int {\n"
            "    while (x > 0) {\n"
            "        return 1;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("while (true) with break statement emits E5001") {
        ParseResult res(
            "fn search(): int {\n"
            "    while (true) {\n"
            "        break;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("Switch without default case emits E5001") {
        ParseResult res(
            "fn pick(x: int): string {\n"
            "    switch (x) {\n"
            "        case (1) { return \"one\"; }\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("Switch with break in default case emits E5001") {
        ParseResult res(
            "fn pick(x: int): string {\n"
            "    switch (x) {\n"
            "        case (1) { return \"one\"; }\n"
            "        default { break; }\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("Class method missing return statement emits E5001") {
        ParseResult res(
            "class Calculator {\n"
            "    pub fn getFactor(): float {}\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }

    SECTION("Non-void closure literal missing return emits E5001") {
        ParseResult res(
            "fn run(): void {\n"
            "    (fn(): int {})();\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::MissingReturnStatement));
    }
}

TEST_CASE("Exhaustiveness Analysis - Exhaustive Control Paths Succeed", "[cfa][exhaustiveness]") {
    SECTION("Straight-line return in non-void function succeeds") {
        ParseResult res("fn add(a: int, b: int): int { return a + b; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("if-else with returns in both branches succeeds") {
        ParseResult res(
            "fn abs(x: int): int {\n"
            "    if (x >= 0) {\n"
            "        return x;\n"
            "    } else {\n"
            "        return -x;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("if-elif-else with returns in all branches succeeds") {
        ParseResult res(
            "fn sign(x: int): int {\n"
            "    if (x > 0) {\n"
            "        return 1;\n"
            "    } elif (x < 0) {\n"
            "        return -1;\n"
            "    } else {\n"
            "        return 0;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Diverging zonk statement satisfies non-void function termination") {
        ParseResult res("fn panic(): int { zonk \"fatal error\"; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("if-else with return in then and zonk in else succeeds") {
        ParseResult res(
            "fn checkPos(x: int): int {\n"
            "    if (x > 0) {\n"
            "        return x;\n"
            "    } else {\n"
            "        zonk \"must be positive\";\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Unconditional while (true) loop without break satisfies termination") {
        ParseResult res(
            "fn serverLoop(): int {\n"
            "    while (true) {\n"
            "        let tick: int = 1;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Unconditional for (;;) loop without break satisfies termination") {
        ParseResult res(
            "fn ticker(): int {\n"
            "    for (;;) {\n"
            "        let tick: int = 1;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Break in inner loop does not break outer infinite loop") {
        ParseResult res(
            "fn worker(): int {\n"
            "    while (true) {\n"
            "        while (true) {\n"
            "            break;\n"
            "        }\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Switch with default where all branches return succeeds") {
        ParseResult res(
            "fn mapNum(x: int): string {\n"
            "    switch (x) {\n"
            "        case (1) { return \"one\"; }\n"
            "        case (2) { return \"two\"; }\n"
            "        default { return \"other\"; }\n"
            "    }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Void functions do not require return statements") {
        ParseResult res("fn logMessage(): void { let x: int = 42; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Void functions with bare return succeed") {
        ParseResult res("fn earlyExit(): void { return; }");
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Class method with return succeeds") {
        ParseResult res(
            "class MathUtil {\n"
            "    pub fn square(n: int): int { return n * n; }\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }

    SECTION("Non-void closure literal with return succeeds") {
        ParseResult res(
            "fn run(): void {\n"
            "    (fn(): int { return 42; })();\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.emitter.hasErrors());
    }
}

TEST_CASE("Reachability Analysis - Dead Code Detection (E5002)", "[cfa][reachability][E5002]") {
    SECTION("Statement following return in same block emits E5002") {
        ParseResult res(
            "fn test(): int {\n"
            "    return 1;\n"
            "    let dead: int = 2;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));

        const auto& d = res.emitter.getDiagnostics().front();
        REQUIRE(d.code == "E5002");
        REQUIRE(d.message.find("unreachable code detected") != std::string::npos);
    }

    SECTION("Statement following zonk statement in same block emits E5002") {
        ParseResult res(
            "fn test(): void {\n"
            "    zonk \"error\";\n"
            "    let dead: int = 2;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }

    SECTION("Statement following break in loop emits E5002") {
        ParseResult res(
            "fn test(): void {\n"
            "    while (true) {\n"
            "        break;\n"
            "        let dead: int = 2;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }

    SECTION("Statement following continue in loop emits E5002") {
        ParseResult res(
            "fn test(): void {\n"
            "    for (let i: int = 0; i < 10; ++i) {\n"
            "        continue;\n"
            "        let dead: int = 2;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }

    SECTION("Statement following if-else where both branches return emits E5002") {
        ParseResult res(
            "fn test(flag: bool): int {\n"
            "    if (flag) {\n"
            "        return 1;\n"
            "    } else {\n"
            "        return 2;\n"
            "    }\n"
            "    return 3;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }

    SECTION("Statement following unconditional infinite loop emits E5002") {
        ParseResult res(
            "fn test(): void {\n"
            "    while (true) {\n"
            "        let x: int = 1;\n"
            "    }\n"
            "    let dead: int = 2;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }

    SECTION("Dead code inside nested block emits E5002") {
        ParseResult res(
            "fn test(): void {\n"
            "    {\n"
            "        return;\n"
            "        let dead: int = 1;\n"
            "    }\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::UnreachableCode));
    }
}

// =============================================================================
// Validate l-values: reject r-values and literals passed to mut (E4005)
// =============================================================================

TEST_CASE("Mutable Reference Borrow Checker - Validate L-Values (E4005)", "[typechecker][borrow_checker][E4005]") {
    SECTION("Reject integer literal passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(mut 10);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject string literal passed to mut parameter") {
        ParseResult res(
            "fn f(mut s: string): void {}\n"
            "fn test(): void {\n"
            "    f(mut \"hello\");\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject boolean literal passed to mut parameter") {
        ParseResult res(
            "fn f(mut b: bool): void {}\n"
            "fn test(): void {\n"
            "    f(mut true);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject array literal passed to mut parameter") {
        ParseResult res(
            "fn f(mut a: array<int>): void {}\n"
            "fn test(): void {\n"
            "    f(mut [1, 2]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject binary expression passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let a: int = 1;\n"
            "    let b: int = 2;\n"
            "    f(mut (a + b));\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject unary expression passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let a: int = 1;\n"
            "    f(mut -a);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject function call return temporary passed to mut parameter") {
        ParseResult res(
            "fn getVal(): int { return 1; }\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(mut getVal());\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject type cast temporary passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let a: float = 1.0;\n"
            "    f(mut (a as int));\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject const binding passed to mut parameter") {
        ParseResult res(
            "const MAX: int = 100;\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(mut MAX);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject define constant passed to mut parameter") {
        ParseResult res(
            "define PI: float = 3.14;\n"
            "fn f(mut x: float): void {}\n"
            "fn test(): void {\n"
            "    f(mut PI);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject immutable (non-mut) parameter forwarded to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn caller(p: int): void {\n"
            "    f(mut p);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject element of const array (deep immutability)") {
        ParseResult res(
            "const arr: array<int> = [1, 2, 3];\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(mut arr[0]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject element of temporary array literal") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(mut [1, 2][0]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject const class field passed to mut parameter") {
        ParseResult res(
            "class Config {\n"
            "    pub const max: int = 100;\n"
            "    pub let current: int = 0;\n"
            "}\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let c: Config = new Config();\n"
            "    f(mut c.max);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject field of const class instance passed to mut parameter") {
        ParseResult res(
            "class Config {\n"
            "    pub let current: int = 0;\n"
            "}\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    const c: Config = new Config();\n"
            "    f(mut c.current);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject non-l-value without mut keyword at call site matching mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    f(10);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject non-l-value in class method call taking mut parameter") {
        ParseResult res(
            "class Modifier {\n"
            "    pub fn modify(mut val: int): void {}\n"
            "}\n"
            "fn test(): void {\n"
            "    let m: Modifier = new Modifier();\n"
            "    m.modify(10);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Reject non-l-value in closure with mut parameter") {
        ParseResult res(
            "fn test(): void {\n"
            "    let cb: (mut x: int) => void = fn(mut x: int): void {};\n"
            "    cb(mut 42);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mutable let variable passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let a: int = 10;\n"
            "    f(mut a);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mutable let variable passed without mut keyword at call site") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let a: int = 10;\n"
            "    f(a);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mut parameter forwarded to another mut parameter") {
        ParseResult res(
            "fn target(mut x: int): void {}\n"
            "fn forwarder(mut p: int): void {\n"
            "    target(mut p);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mutable array element passed to mut parameter") {
        ParseResult res(
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [10, 20];\n"
            "    f(mut arr[0]);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mutable class field passed to mut parameter") {
        ParseResult res(
            "class Player {\n"
            "    pub let hp: int = 100;\n"
            "}\n"
            "fn f(mut x: int): void {}\n"
            "fn test(): void {\n"
            "    let p: Player = new Player();\n"
            "    f(mut p.hp);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }

    SECTION("Accept mutable class field in method call") {
        ParseResult res(
            "class Modifier {\n"
            "    pub fn modify(mut target: int): void {}\n"
            "}\n"
            "fn test(): void {\n"
            "    let m: Modifier = new Modifier();\n"
            "    let val: int = 5;\n"
            "    m.modify(mut val);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::NonLValueMutableBorrow));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker - Exclusive Borrowing (E4001)
// =============================================================================

TEST_CASE("Mutable Reference Borrow Checker - Exclusive Borrowing (E4001)", "[typechecker][borrow_checker][E4001]") {
    SECTION("Reject duplicate borrow of same variable with explicit mut (neg_mut_alias)") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {\n"
            "    let temp: int = a;\n"
            "    a = b;\n"
            "    b = temp;\n"
            "}\n"
            "fn test(): void {\n"
            "    let x: int = 42;\n"
            "    swap(mut x, mut x);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow without mut keyword at call site") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let x: int = 10;\n"
            "    swap(x, x);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow in function with three or more parameters") {
        ParseResult res(
            "fn process(mut a: int, mut b: int, mut c: int): void {}\n"
            "fn test(): void {\n"
            "    let x: int = 1;\n"
            "    let y: int = 2;\n"
            "    process(mut x, mut y, mut x);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow of the same class field") {
        ParseResult res(
            "class Player {\n"
            "    pub let hp: int = 100;\n"
            "    pub let mana: int = 50;\n"
            "}\n"
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let p: Player = new Player();\n"
            "    swap(mut p.hp, mut p.hp);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept distinct class fields on the same object") {
        ParseResult res(
            "class Player {\n"
            "    pub let hp: int = 100;\n"
            "    pub let mana: int = 50;\n"
            "}\n"
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let p: Player = new Player();\n"
            "    swap(mut p.hp, mut p.mana);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept same field on two distinct object instances") {
        ParseResult res(
            "class Player {\n"
            "    pub let hp: int = 100;\n"
            "}\n"
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let p1: Player = new Player();\n"
            "    let p2: Player = new Player();\n"
            "    swap(mut p1.hp, mut p2.hp);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject overlapping borrow of object and its field") {
        ParseResult res(
            "class Inner {\n"
            "    pub let val: int = 0;\n"
            "}\n"
            "class Outer {\n"
            "    pub let inner: Inner = new Inner();\n"
            "}\n"
            "fn f(mut o: Outer, mut i: Inner): void {}\n"
            "fn test(): void {\n"
            "    let o: Outer = new Outer();\n"
            "    f(mut o, mut o.inner);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow of the same array element") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2];\n"
            "    swap(mut arr[0], mut arr[0]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept distinct array elements") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2];\n"
            "    swap(mut arr[0], mut arr[1]);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow in class method call") {
        ParseResult res(
            "class MathOps {\n"
            "    pub fn exchange(mut a: int, mut b: int): void {}\n"
            "}\n"
            "fn test(): void {\n"
            "    let m: MathOps = new MathOps();\n"
            "    let x: int = 10;\n"
            "    m.exchange(mut x, mut x);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject duplicate borrow in closure call") {
        ParseResult res(
            "fn test(): void {\n"
            "    let cb: (mut a: int, mut b: int) => void = fn(mut a: int, mut b: int): void {};\n"
            "    let x: int = 10;\n"
            "    cb(mut x, mut x);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept distinct mutable variables in call") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let x: int = 1;\n"
            "    let y: int = 2;\n"
            "    swap(mut x, mut y);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept multiple sequential calls borrowing the same variable") {
        ParseResult res(
            "fn inc(mut a: int): void {}\n"
            "fn test(): void {\n"
            "    let x: int = 1;\n"
            "    inc(mut x);\n"
            "    inc(mut x);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Accept forwarding two distinct mut parameters") {
        ParseResult res(
            "fn target(mut a: int, mut b: int): void {}\n"
            "fn forwarder(mut p1: int, mut p2: int): void {\n"
            "    target(mut p1, mut p2);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }

    SECTION("Reject forwarding the same mut parameter twice") {
        ParseResult res(
            "fn target(mut a: int, mut b: int): void {}\n"
            "fn forwarder(mut p: int): void {\n"
            "    target(mut p, mut p);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::DuplicateMutableBorrow));
    }
}

// =============================================================================
// Mutable Reference Borrow Checker - Container and Element Borrowing (E4002)
// =============================================================================

TEST_CASE("Mutable Reference Borrow Checker - Container and Element Borrowing (E4002)", "[typechecker][borrow_checker][E4002]") {
    SECTION("Reject simultaneous borrow of element and container: f(mut arr[i], mut arr)") {
        ParseResult res(
            "fn modify(mut item: int, mut arr: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    let i: int = 0;\n"
            "    modify(mut arr[i], mut arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject simultaneous borrow with reverse argument order: f(mut arr, mut arr[i])") {
        ParseResult res(
            "fn modify(mut arr: array<int>, mut item: int): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    let i: int = 0;\n"
            "    modify(mut arr, mut arr[i]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject when element is mut and container is by-value: f(mut arr[i], arr)") {
        ParseResult res(
            "fn check(mut item: int, arr: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    let i: int = 0;\n"
            "    check(mut arr[i], arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject when container is mut and element is by-value: f(arr[i], mut arr)") {
        ParseResult res(
            "fn check(item: int, mut arr: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    let i: int = 0;\n"
            "    check(arr[i], mut arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject when callee parameters are declared mut but call site omits mut") {
        ParseResult res(
            "fn modify(mut item: int, mut arr: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    let i: int = 0;\n"
            "    modify(arr[i], arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject nested container indexing against outer container: matrix[0][1] and matrix") {
        ParseResult res(
            "fn mutateNested(mut cell: int, mut m: array<array<int>>): void {}\n"
            "fn test(): void {\n"
            "    let matrix: array<array<int>> = [[1, 2], [3, 4]];\n"
            "    mutateNested(mut matrix[0][1], mut matrix);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject nested container indexing against intermediate container: matrix[0][1] and matrix[0]") {
        ParseResult res(
            "fn mutateRow(mut cell: int, mut row: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let matrix: array<array<int>> = [[1, 2], [3, 4]];\n"
            "    mutateRow(mut matrix[0][1], mut matrix[0]);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject container and element borrowing in class method call") {
        ParseResult res(
            "class Worker {\n"
            "    pub fn process(mut item: int, mut arr: array<int>): void {}\n"
            "}\n"
            "fn test(): void {\n"
            "    let w: Worker = new Worker();\n"
            "    let arr: array<int> = [10, 20];\n"
            "    w.process(mut arr[0], mut arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject container and element borrowing in closure call") {
        ParseResult res(
            "fn test(): void {\n"
            "    let cb: (mut a: int, mut b: array<int>) => void = fn(mut a: int, mut b: array<int>): void {};\n"
            "    let arr: array<int> = [10, 20];\n"
            "    cb(mut arr[0], mut arr);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Reject member container and its element: obj.items[0] and obj.items") {
        ParseResult res(
            "class ContainerHolder {\n"
            "    pub let items: array<int> = [1, 2, 3];\n"
            "}\n"
            "fn inspect(mut item: int, mut list: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let holder: ContainerHolder = new ContainerHolder();\n"
            "    inspect(mut holder.items[0], mut holder.items);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkZeroCoercion());
        REQUIRE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Accept distinct containers: arr1[0] and arr2") {
        ParseResult res(
            "fn operate(mut item: int, mut list: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr1: array<int> = [1, 2];\n"
            "    let arr2: array<int> = [3, 4];\n"
            "    operate(mut arr1[0], mut arr2);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Accept distinct elements of same container: arr[0] and arr[1]") {
        ParseResult res(
            "fn swap(mut a: int, mut b: int): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2];\n"
            "    swap(mut arr[0], mut arr[1]);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Accept container and element passed by value (neither is mut)") {
        ParseResult res(
            "fn examine(item: int, list: array<int>): int {\n"
            "    return item + list.length;\n"
            "}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    examine(arr[0], arr);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Accept sequential calls borrowing container and element separately") {
        ParseResult res(
            "fn modifyElem(mut item: int): void {}\n"
            "fn modifyCont(mut list: array<int>): void {}\n"
            "fn test(): void {\n"
            "    let arr: array<int> = [1, 2, 3];\n"
            "    modifyElem(mut arr[0]);\n"
            "    modifyCont(mut arr);\n"
            "}\n"
        );
        REQUIRE(res.checkZeroCoercion());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::ContainerAndElementBorrow));
    }

    SECTION("Verify diagnostic structure, labels, notes, and help footer") {
        diag::SourceLocation loc1{10, 5, 100};
        diag::SourceLocation loc2{10, 15, 110};
        diag::SourceSpan span1{loc1, loc2};

        diag::SourceLocation loc3{10, 20, 115};
        diag::SourceLocation loc4{10, 30, 125};
        diag::SourceSpan span2{loc3, loc4};

        auto diag = typechecker::TypeChecker::createContainerAndElementBorrowDiagnostic(
            "arr", "arr[i]", span1, span2);

        REQUIRE(diag.code == "E4002");
        REQUIRE(diag.message == "cannot simultaneously borrow container 'arr' and its element 'arr[i]'");
        REQUIRE(diag.labels.size() == 2);
        REQUIRE(diag.labels[0].isPrimary);
        REQUIRE(diag.labels[0].message == "simultaneous borrow of container and element occurs here");
        REQUIRE_FALSE(diag.labels[1].isPrimary);
        REQUIRE(diag.labels[1].message == "first borrow occurs here");
        REQUIRE(diag.footers.size() == 2);
        REQUIRE(diag.footers[0].kind == diag::DiagnosticFooter::Kind::Note);
        REQUIRE(diag.footers[0].message == "Simultaneous mutable borrowing of a container and one of its elements violates disjointness.");
        REQUIRE(diag.footers[1].kind == diag::DiagnosticFooter::Kind::Help);
        REQUIRE(diag.footers[1].message == "container mutations can reallocate storage and invalidate active element references");
    }
}



