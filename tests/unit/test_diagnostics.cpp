#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/levenshtein.hpp>
#include <keety/diag/source_manager.hpp>

using namespace keety::diag;

TEST_CASE("Levenshtein distance calculation", "[diag][levenshtein]") {
    SECTION("Exact match has distance 0") {
        REQUIRE(levenshtein_distance("println", "println") == 0);
        REQUIRE(levenshtein_distance("", "") == 0);
    }

    SECTION("Empty strings distance to non-empty") {
        REQUIRE(levenshtein_distance("", "abc") == 3);
        REQUIRE(levenshtein_distance("abcd", "") == 4);
    }

    SECTION("Single edit insertions, deletions, and substitutions") {
        REQUIRE(levenshtein_distance("print", "println") == 2);
        REQUIRE(levenshtein_distance("print", "prit") == 1);
        REQUIRE(levenshtein_distance("print", "pront") == 1);
    }

    SECTION("Classic Wagner-Fischer examples") {
        REQUIRE(levenshtein_distance("kitten", "sitting") == 3);
        REQUIRE(levenshtein_distance("flaw", "lawn") == 2);
    }
}

TEST_CASE("Fuzzy typo candidate matcher", "[diag][levenshtein]") {
    const std::vector<std::string> candidates = {
        "print", "println", "eprint", "eprintln", "readln", "readLine", "input", "flush"
    };

    SECTION("Finds closest matching identifier") {
        auto match = find_closest_match("prntln", candidates, 2);
        REQUIRE(match.has_value());
        REQUIRE(*match == "println");
    }

    SECTION("Returns nullopt when beyond threshold") {
        auto match = find_closest_match("completelyDifferent", candidates, 3);
        REQUIRE_FALSE(match.has_value());
    }
}

TEST_CASE("Naming convention heuristics: snake_case and camelCase", "[diag][levenshtein]") {
    SECTION("is_snake_case identification") {
        REQUIRE(is_snake_case("read_text"));
        REQUIRE(is_snake_case("to_string"));
        REQUIRE_FALSE(is_snake_case("readText"));
        REQUIRE_FALSE(is_snake_case("ReadText"));
        REQUIRE_FALSE(is_snake_case("simple"));
    }

    SECTION("to_camel_case conversion") {
        REQUIRE(to_camel_case("read_text") == "readText");
        REQUIRE(to_camel_case("now_ms") == "nowMs");
        REQUIRE(to_camel_case("format_iso_8601") == "formatIso8601");
    }

    SECTION("to_snake_case conversion") {
        REQUIRE(to_snake_case("readText") == "read_text");
        REQUIRE(to_snake_case("nowMs") == "now_ms");
    }
}

TEST_CASE("Diagnostic emitter multi-line footer formatting", "[diag][emitter]") {
    SourceManager sm;
    auto fid = sm.addBuffer("test.keet", "let x: int = \"hello\";\n");
    DiagnosticEmitter emitter(sm, ColorMode::Never);

    Diagnostic d = Diagnostic::makeError("E3003", "type mismatch", SourceLocation{fid, 13, 1, 14});
    d.withLabel(SourceSpan{SourceLocation{fid, 13, 1, 14}, SourceLocation{fid, 20, 1, 21}}, "expected int, found string");
    d.withNote("KeetScript requires exact static type matching.");
    d.withHelp("to convert from string, use:\nlet x: int = \"hello\".toInt();");

    std::string output = emitter.format(d);

    // Verify gutter alignment and help indentation
    REQUIRE(output.find("error[E3003]: type mismatch") != std::string::npos);
    REQUIRE(output.find(" = note: KeetScript requires exact static type matching.") != std::string::npos);
    REQUIRE(output.find(" = help: to convert from string, use:") != std::string::npos);
    REQUIRE(output.find("         let x: int = \"hello\".toInt();") != std::string::npos);
}

TEST_CASE("Diagnostic emitter primary and secondary label priority", "[diag][emitter]") {
    SourceManager sm;
    auto fid = sm.addBuffer("borrow.keet", "let mut x = 10;\nlet p = &mut x;\nlet q = &mut x;\n");
    DiagnosticEmitter emitter(sm, ColorMode::Never);

    Diagnostic d = Diagnostic::makeError("E4001", "cannot borrow 'x' as mutable more than once at a time", SourceLocation{fid, 33, 3, 9});
    d.withSecondaryLabel(SourceSpan{SourceLocation{fid, 24, 2, 9}, SourceLocation{fid, 30, 2, 15}}, "first mutable borrow occurs here");
    d.withLabel(SourceSpan{SourceLocation{fid, 40, 3, 9}, SourceLocation{fid, 46, 3, 15}}, "second mutable borrow occurs here");

    std::string output = emitter.format(d);

    // Primary label on line 3 uses ^
    REQUIRE(output.find("^^^^^^ second mutable borrow occurs here") != std::string::npos);
    // Secondary label on line 2 uses -
    REQUIRE(output.find("------ first mutable borrow occurs here") != std::string::npos);
}

#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/typechecker/type_checker.hpp>

namespace {
struct DiagTestHelper {
    SourceManager sm;
    DiagnosticEmitter emitter;
    keety::parser::ModuleAST ast;

    explicit DiagTestHelper(std::string_view src) : emitter(sm, ColorMode::Never) {
        auto fid = sm.addBuffer("test.keet", std::string(src));
        keety::lexer::Lexer lexer(fid, src, &emitter);
        keety::parser::Parser parser(lexer, &emitter);
        ast = parser.parseModule("test");
    }

    bool checkTypes() {
        keety::typechecker::TypeChecker tc(&emitter);
        return tc.checkTypes(ast);
    }

    bool hasDiag(std::string_view code) const {
        for (const auto& d : emitter.getDiagnostics()) {
            if (d.code == code) return true;
        }
        return false;
    }

    bool hasFooterContaining(DiagnosticFooter::Kind kind, std::string_view substring) const {
        for (const auto& d : emitter.getDiagnostics()) {
            for (const auto& f : d.footers) {
                if (f.kind == kind && f.message.find(substring) != std::string::npos) {
                    return true;
                }
            }
        }
        return false;
    }
};
} // namespace

TEST_CASE("Type checker typo suggestions and camelCase hint", "[diag][typechecker]") {
    SECTION("Fuzzy match suggests closest standard library method") {
        DiagTestHelper h("import io;\nfn main(): void {\n    io.prntln(\"hello\");\n}\n");
        REQUIRE_FALSE(h.checkTypes());
        REQUIRE(h.hasDiag("E3011"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "did you mean 'println'?"));
    }

    SECTION("snake_case access emits camelCase naming convention hint") {
        DiagTestHelper h("import fs;\nfn main(): void {\n    fs.read_text(\"foo.txt\");\n}\n");
        REQUIRE_FALSE(h.checkTypes());
        REQUIRE(h.hasDiag("E3011"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Note, "camelCase"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "did you mean 'readText'?"));
    }

    SECTION("Undeclared module identifier emits import hint") {
        DiagTestHelper h("fn main(): void {\n    io.println(\"hello\");\n}\n");
        REQUIRE_FALSE(h.checkTypes());
        REQUIRE(h.hasDiag("E2003"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "add 'import io;' at the top of the file"));
    }
}

TEST_CASE("Type checker nullable union and zero coercion hints", "[diag][typechecker]") {
    SECTION("Nullable union assignment explains required flow narrowing") {
        DiagTestHelper h(
            "fn main(): void {\n"
            "    let a: int | null = null;\n"
            "    let b: int = a;\n"
            "}\n"
        );
        REQUIRE_FALSE(h.checkTypes());
        REQUIRE(h.hasDiag("E3003"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Note, "nullable union type"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "narrow the type first with 'if (x != null)'"));
    }

    SECTION("Float from int strict zero coercion suggests literal or .toFloat()") {
        DiagTestHelper h(
            "fn main(): void {\n"
            "    let x: float = 42;\n"
            "}\n"
        );
        REQUIRE_FALSE(h.checkTypes());
        REQUIRE(h.hasDiag("E3002"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "float literal like '42.0'"));
    }
}

TEST_CASE("Parser context-aware error hints and warnings", "[diag][parser]") {
    SECTION("Assignment '=' in if-condition emits warning W1001") {
        DiagTestHelper h(
            "fn main(): void {\n"
            "    let mut x: int = 5;\n"
            "    if (x = 10) {}\n"
            "}\n"
        );
        REQUIRE(h.hasDiag("W1001"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "did you mean '==' for comparison?"));
    }

    SECTION("Missing ':' in variable type annotation emits expected colon with help") {
        DiagTestHelper h(
            "fn main(): void {\n"
            "    let x int = 5;\n"
            "}\n"
        );
        REQUIRE(h.hasDiag("E0012"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "add ':' between variable name and type annotation"));
    }

    SECTION("Missing '(' in function call argument list emits E0015") {
        DiagTestHelper h(
            "import io;\n"
            "fn main(): void {\n"
            "    io.print\"Hello, World!\");\n"
            "}\n"
        );
        REQUIRE(h.hasDiag("E0015"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "add '(' to call 'io.print'"));
    }

    SECTION("Unparenthesized function call with argument emits E0015") {
        DiagTestHelper h(
            "import io;\n"
            "fn main(): void {\n"
            "    io.print \"Hello, World!\";\n"
            "}\n"
        );
        REQUIRE(h.hasDiag("E0015"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "add '(' to call 'io.print'"));
    }

    SECTION("Unmatched closing ')' in expression statement") {
        DiagTestHelper h(
            "fn main(): void {\n"
            "    let x: int = 5;\n"
            "    x);\n"
            "}\n"
        );
        REQUIRE(h.hasDiag("E0005"));
        REQUIRE(h.hasFooterContaining(DiagnosticFooter::Kind::Help, "remove the extra ')' or add an opening '('"));
    }
}

