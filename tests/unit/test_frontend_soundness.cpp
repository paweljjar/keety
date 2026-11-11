#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/typechecker/type_checker.hpp>

using namespace keety;

namespace {

struct SoundnessCheckResult {
    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter;
    parser::ModuleAST ast;

    SoundnessCheckResult(std::string_view source) : emitter(sm, diag::ColorMode::Never) {
        auto fileId = sm.addBuffer("soundness_test.keet", std::string(source));
        lexer::Lexer lexer(fileId, source, &emitter);
        parser::Parser parser(lexer, &emitter);
        ast = parser.parseModule("soundness_test");
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

    [[nodiscard]] std::string getFormattedDiagnostics() const {
        std::string out;
        for (const auto& d : emitter.getDiagnostics()) {
            out += emitter.format(d) + "\n";
        }
        return out;
    }

    bool checkTypes() {
        typechecker::TypeChecker checker(&emitter);
        return checker.checkZeroCoercion(ast);
    }
};

} // namespace

TEST_CASE("Frontend Soundness: Explicit Cast Validation (E3010)", "[typechecker][soundness][E3010]") {
    SECTION("Valid numeric casts succeed") {
        SoundnessCheckResult res(
            "fn convert(x: int): float {\n"
            "    return x as float;\n"
            "}\n"
        );
        REQUIRE(res.checkTypes());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::InvalidTypeCast));
    }

    SECTION("Invalid primitive string-to-int cast is rejected with E3010") {
        SoundnessCheckResult res(
            "fn invalidCast(s: string): int {\n"
            "    return s as int;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidTypeCast));
        REQUIRE(res.getFormattedDiagnostics().find("invalid type cast from 'string' to 'int'") != std::string::npos);
    }

    SECTION("Invalid primitive to array cast is rejected with E3010") {
        SoundnessCheckResult res(
            "fn invalidCast(x: int): array<int> {\n"
            "    return x as array<int>;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidTypeCast));
    }

    SECTION("Valid union narrowing cast succeeds") {
        SoundnessCheckResult res(
            "fn unwrap(v: int | string): int {\n"
            "    return v as int;\n"
            "}\n"
        );
        REQUIRE(res.checkTypes());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::InvalidTypeCast));
    }

    SECTION("Invalid unrelated custom types cast is rejected with E3010") {
        SoundnessCheckResult res(
            "class Cat {\n"
            "    pub let name: string;\n"
            "}\n"
            "class Dog {\n"
            "    pub let barkVolume: int;\n"
            "}\n"
            "fn convertPet(c: Cat): Dog {\n"
            "    return c as Dog;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidTypeCast));
    }
}

TEST_CASE("Frontend Soundness: Class Constructor Argument Count & Type Soundness (E3003)", "[typechecker][soundness][E3003]") {
    SECTION("Default constructor with 0 arguments succeeds") {
        SoundnessCheckResult res(
            "class Point {\n"
            "    pub let x: int;\n"
            "    pub let y: int;\n"
            "}\n"
            "fn test(): Point {\n"
            "    return Point();\n"
            "}\n"
        );
        REQUIRE(res.checkTypes());
        REQUIRE_FALSE(res.hasError(diag::ErrorCode::TypeMismatch));
    }

    SECTION("Constructor called with too many arguments emits E3003") {
        SoundnessCheckResult res(
            "class Point {\n"
            "    pub let x: int;\n"
            "    pub let y: int;\n"
            "}\n"
            "fn test(): Point {\n"
            "    return Point(10, 20, 30);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
        REQUIRE(res.getFormattedDiagnostics().find("class 'Point' constructor expects at most 2 argument(s), but 3 were supplied") != std::string::npos);
    }

    SECTION("Constructor called with argument type mismatch emits E3003") {
        SoundnessCheckResult res(
            "class Point {\n"
            "    pub let x: int;\n"
            "    pub let y: int;\n"
            "}\n"
            "fn test(): Point {\n"
            "    return Point(10, \"twenty\");\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
        REQUIRE(res.getFormattedDiagnostics().find("expected 'int'") != std::string::npos);
        REQUIRE(res.getFormattedDiagnostics().find("constructor argument 'y'") != std::string::npos);
    }
}

TEST_CASE("Frontend Soundness: Function and Method Argument Count Validation (E3003)", "[typechecker][soundness][call_args]") {
    SECTION("Function called with too few arguments emits E3003") {
        SoundnessCheckResult res(
            "fn add(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "fn test(): int {\n"
            "    return add(5);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
        REQUIRE(res.getFormattedDiagnostics().find("function 'add' expects 2 argument(s), but 1 were supplied") != std::string::npos);
    }

    SECTION("Function called with too many arguments emits E3003") {
        SoundnessCheckResult res(
            "fn add(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "fn test(): int {\n"
            "    return add(5, 10, 15);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
        REQUIRE(res.getFormattedDiagnostics().find("function 'add' expects 2 argument(s), but 3 were supplied") != std::string::npos);
    }

    SECTION("Method called with argument count mismatch emits E3003") {
        SoundnessCheckResult res(
            "class Calculator {\n"
            "    pub fn compute(a: int, b: int): int {\n"
            "        return a * b;\n"
            "    }\n"
            "}\n"
            "fn test(calc: Calculator): int {\n"
            "    return calc.compute(5);\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::TypeMismatch));
        REQUIRE(res.getFormattedDiagnostics().find("method 'compute' expects 2 argument(s), but 1 were supplied") != std::string::npos);
    }
}

TEST_CASE("Frontend Soundness: Member Access & Levenshtein Suggestions (E3011)", "[typechecker][soundness][E3011]") {
    SECTION("Unknown method call on class emits E3011 with did-you-mean suggestion") {
        SoundnessCheckResult res(
            "class Hero {\n"
            "    pub fn attack(): int {\n"
            "        return 100;\n"
            "    }\n"
            "}\n"
            "fn test(h: Hero): int {\n"
            "    return h.attak();\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidMemberAccess));
        REQUIRE(res.getFormattedDiagnostics().find("did you mean 'attack'?") != std::string::npos);
    }

    SECTION("Calling a field as a method emits E3011 with clear suggestion") {
        SoundnessCheckResult res(
            "class Player {\n"
            "    pub let score: int;\n"
            "}\n"
            "fn test(p: Player): int {\n"
            "    return p.score();\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidMemberAccess));
        REQUIRE(res.getFormattedDiagnostics().find("is a field, not a method; did you mean to access it without '()'") != std::string::npos);
    }

    SECTION("Unknown field access on class emits E3011 with did-you-mean suggestion") {
        SoundnessCheckResult res(
            "class Entity {\n"
            "    pub let health: int;\n"
            "}\n"
            "fn test(e: Entity): int {\n"
            "    return e.helth;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidMemberAccess));
        REQUIRE(res.getFormattedDiagnostics().find("did you mean 'health'?") != std::string::npos);
    }

    SECTION("Accessing a method as a field emits E3011 with call suggestion") {
        SoundnessCheckResult res(
            "class Engine {\n"
            "    pub fn start(): bool {\n"
            "        return true;\n"
            "    }\n"
            "}\n"
            "fn test(e: Engine): bool {\n"
            "    return e.start;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidMemberAccess));
        REQUIRE(res.getFormattedDiagnostics().find("is a method, not a field; did you mean to call it as 'start()'") != std::string::npos);
    }

    SECTION("Member access on primitive type emits E3011") {
        SoundnessCheckResult res(
            "fn test(x: int): int {\n"
            "    return x.invalidField;\n"
            "}\n"
        );
        REQUIRE_FALSE(res.checkTypes());
        INFO("Diagnostics: " << res.getFormattedDiagnostics());
        REQUIRE(res.hasError(diag::ErrorCode::InvalidMemberAccess));
        REQUIRE(res.getFormattedDiagnostics().find("primitive type 'int'") != std::string::npos);
    }
}
