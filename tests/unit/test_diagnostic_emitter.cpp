#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>

using namespace keety::diag;

TEST_CASE("DiagnosticEmitter simple error formatting without colors", "[diag]") {
    SourceManager sm;
    FileId fid = sm.addBuffer("src/test.keet", "let a: int = 10;\nlet b = 20;\n");

    DiagnosticEmitter emitter(sm, ColorMode::Never);

    SourceLocation loc = sm.resolveLocation(fid, 17); // line 2, col 1: "let b = 20;"
    SourceLocation locEnd = sm.resolveLocation(fid, 22); // "let b"
    SourceSpan span{loc, locEnd};

    Diagnostic diag = Diagnostic::makeError("E3001", "missing explicit type annotation", loc);
    diag.withLabel(span, "type annotation required here")
        .withNote("KeetScript requires explicit type annotations on all declarations")
        .withHelp("specify a type, e.g. 'let b: int = 20;'");

    std::string formatted = emitter.format(diag);

    REQUIRE(formatted.find("error[E3001]: missing explicit type annotation") != std::string::npos);
    REQUIRE(formatted.find("--> src/test.keet:2:1") != std::string::npos);
    REQUIRE(formatted.find("2 | let b = 20;") != std::string::npos);
    REQUIRE(formatted.find("^^^^^ type annotation required here") != std::string::npos);
    REQUIRE(formatted.find("= note: KeetScript requires explicit type annotations on all declarations") != std::string::npos);
    REQUIRE(formatted.find("= help: specify a type, e.g. 'let b: int = 20;'") != std::string::npos);
}

TEST_CASE("DiagnosticEmitter multi-label borrowing error matching architecture spec", "[diag]") {
    SourceManager sm;
    std::string code =
        "fn process() {\n"
        "    swapBuffers(mut bufferPool, mut bufferPool);\n"
        "}\n";
    FileId fid = sm.addBuffer("src/network/router.keet", code);

    DiagnosticEmitter emitter(sm, ColorMode::Never);

    // swapBuffers(mut bufferPool, mut bufferPool);
    // line 2:
    // start 1: "mut bufferPool" (col 17 to 31)
    // start 2: "mut bufferPool" (col 33 to 47)
    SourceLocation loc1Start{fid, 35, 2, 17};
    SourceLocation loc1End{fid, 49, 2, 31};
    SourceSpan span1{loc1Start, loc1End};

    SourceLocation loc2Start{fid, 51, 2, 33};
    SourceLocation loc2End{fid, 65, 2, 47};
    SourceSpan span2{loc2Start, loc2End};

    Diagnostic diag = Diagnostic::makeError("E4001", "cannot pass multiple mutable references to the same storage path", loc2Start);
    diag.withLabel(span1, "first mutable borrow occurs here", false)
        .withLabel(span2, "duplicate mutable borrow", true)
        .withNote("KeetScript enforces exclusive mutable borrowing to prevent reference aliasing bugs")
        .withHelp("ensure each 'mut' argument references a distinct, disjoint storage location");

    emitter.report(diag);
    REQUIRE(emitter.hasErrors());
    REQUIRE(emitter.getErrorCount() == 1);
    REQUIRE(emitter.getWarningCount() == 0);

    std::string formatted = emitter.format(diag);

    REQUIRE(formatted.find("error[E4001]: cannot pass multiple mutable references to the same storage path") != std::string::npos);
    REQUIRE(formatted.find("--> src/network/router.keet:2:33") != std::string::npos);
    REQUIRE(formatted.find("2 |     swapBuffers(mut bufferPool, mut bufferPool);") != std::string::npos);
    REQUIRE(formatted.find("= note: KeetScript enforces exclusive mutable borrowing to prevent reference aliasing bugs") != std::string::npos);
    REQUIRE(formatted.find("= help: ensure each 'mut' argument references a distinct, disjoint storage location") != std::string::npos);
}
