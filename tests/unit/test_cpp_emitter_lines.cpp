#include <catch2/catch_test_macros.hpp>

#include <keety/diag/source_manager.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/parser/parser.hpp>

#include <string>

using namespace keety;
using namespace keety::emitter;
using namespace keety::diag;

namespace {

struct ParsedUnit {
    SourceManager sm;
    FileId fileId{INVALID_FILE_ID};
    parser::ModuleAST ast;
};

ParsedUnit parseModuleWithSourceManager(const std::string& filePath, const std::string& source, const std::string& modName = "main") {
    ParsedUnit unit;
    unit.fileId = unit.sm.addBuffer(filePath, source);
    DiagnosticEmitter diagEmitter(unit.sm);
    lexer::Lexer lexer(unit.fileId, source, &diagEmitter);
    parser::Parser parser(lexer, &diagEmitter);
    unit.ast = parser.parseModule(modName);
    return unit;
}

} // namespace

TEST_CASE("CppEmitter emits #line directives for functions and statements", "[emitter][line_mapping]") {
    std::string src = 
        "fn calculate(a: int, b: int): int {\n"   // line 1
        "    let sum = a + b;\n"                  // line 2
        "    return sum;\n"                       // line 3
        "}\n";                                    // line 4

    auto unit = parseModuleWithSourceManager("src/calc.keet", src, "calc");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    // Verify #line directives before function and statements
    REQUIRE(cpp.find("#line 1 \"src/calc.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 2 \"src/calc.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 3 \"src/calc.keet\"\n") != std::string::npos);
}

TEST_CASE("CppEmitter deduplicates consecutive statements on the same line", "[emitter][line_mapping]") {
    std::string src = 
        "fn testSameLine(): void {\n"             // line 1
        "    let x = 1; let y = 2;\n"             // line 2 (two statements on line 2)
        "    return;\n"                           // line 3
        "}\n";

    auto unit = parseModuleWithSourceManager("src/same_line.keet", src, "test");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    // Count occurrences of "#line 2 \"src/same_line.keet\""
    std::string target = "#line 2 \"src/same_line.keet\"";
    size_t count = 0;
    size_t pos = cpp.find(target);
    while (pos != std::string::npos) {
        count++;
        pos = cpp.find(target, pos + target.size());
    }

    // Line 2 should be emitted exactly once despite having two statements!
    REQUIRE(count == 1);
    REQUIRE(cpp.find("#line 3 \"src/same_line.keet\"") != std::string::npos);
}

TEST_CASE("CppEmitter emits #line for control flow constructs", "[emitter][line_mapping]") {
    std::string src = 
        "fn flow(x: int): int {\n"                // line 1
        "    if (x > 0) {\n"                      // line 2
        "        return 1;\n"                     // line 3
        "    } else if (x < 0) {\n"               // line 4
        "        return -1;\n"                    // line 5
        "    }\n"                                 // line 6
        "    while (x > 10) {\n"                  // line 7
        "        break;\n"                        // line 8
        "    }\n"                                 // line 9
        "    return 0;\n"                         // line 10
        "}\n";

    auto unit = parseModuleWithSourceManager("src/flow.keet", src, "flow");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    REQUIRE(cpp.find("#line 2 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 3 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 4 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 5 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 7 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 8 \"src/flow.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 10 \"src/flow.keet\"\n") != std::string::npos);
}

TEST_CASE("CppEmitter emits #line for module-level variables", "[emitter][line_mapping]") {
    std::string src = 
        "let counter: int = 100;\n"               // line 1
        "const MAX: int = 500;\n"                 // line 2
        "fn get(): int { return counter; }\n";    // line 3

    auto unit = parseModuleWithSourceManager("src/globals.keet", src, "globals");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    REQUIRE(cpp.find("#line 1 \"src/globals.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 2 \"src/globals.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 3 \"src/globals.keet\"\n") != std::string::npos);
}

TEST_CASE("CppEmitter suppresses #line when emit_line_directives is disabled", "[emitter][line_mapping]") {
    std::string src = 
        "let x: int = 42;\n"
        "fn foo(): int {\n"
        "    return x;\n"
        "}\n";

    auto unit = parseModuleWithSourceManager("src/noline.keet", src, "noline");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = false;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    // No #line directives anywhere
    REQUIRE(cpp.find("#line") == std::string::npos);
}

TEST_CASE("CppEmitter isolates synthetic main wrapper from user line directives", "[emitter][line_mapping]") {
    std::string src = 
        "fn main(): void {\n"                     // line 1
        "    let x = 1;\n"                        // line 2
        "}\n";

    auto unit = parseModuleWithSourceManager("src/main.keet", src, "main");

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;
    opts.is_root_module = true;

    CppEmitter emitter(unit.ast, opts);
    std::string cpp = emitter.emit();

    size_t mainPos = cpp.find("int main(int argc, char* argv[])");
    REQUIRE(mainPos != std::string::npos);

    // After int main(), there should NOT be any user #line directives inside the wrapper
    std::string mainBody = cpp.substr(mainPos);
    REQUIRE(mainBody.find("#line") == std::string::npos);
}

TEST_CASE("HeaderEmitter emits #line directives for exported declarations", "[emitter][line_mapping]") {
    std::string src = 
        "define MAX_BUFFER: int = 1024;\n"        // line 1
        "class Point { x: int, y: int }\n"        // line 2
        "fn add(a: int, b: int): int;\n";         // line 3

    auto unit = parseModuleWithSourceManager("src/geometry.keet", src, "geometry");

    HeaderEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    HeaderEmitter emitter(unit.ast, opts);
    std::string hpp = emitter.emit();

    REQUIRE(hpp.find("#line 1 \"src/geometry.keet\"\n") != std::string::npos);
    REQUIRE(hpp.find("#line 2 \"src/geometry.keet\"\n") != std::string::npos);
    REQUIRE(hpp.find("#line 3 \"src/geometry.keet\"\n") != std::string::npos);
}

TEST_CASE("HeaderEmitter suppresses #line when emit_line_directives is disabled", "[emitter][line_mapping]") {
    std::string src = 
        "define PI: float = 3.14159;\n"
        "class Circle { radius: float }\n";

    auto unit = parseModuleWithSourceManager("src/math.keet", src, "math");

    HeaderEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = false;

    HeaderEmitter emitter(unit.ast, opts);
    std::string hpp = emitter.emit();

    REQUIRE(hpp.find("#line") == std::string::npos);
}
