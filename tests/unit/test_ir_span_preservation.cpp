#include <catch2/catch_test_macros.hpp>

#include <keety/diag/source_manager.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/parser/parser.hpp>

#include <string>

using namespace keety;
using namespace keety::emitter;
using namespace keety::diag;
using namespace keety::ir;

namespace {

struct LoweredUnit {
    SourceManager sm;
    FileId fileId{INVALID_FILE_ID};
    parser::ModuleAST ast;
    std::unique_ptr<Module> irModule;
};

LoweredUnit lowerModuleWithSpans(const std::string& filePath, const std::string& source, const std::string& modName = "main") {
    LoweredUnit unit;
    unit.fileId = unit.sm.addBuffer(filePath, source);
    DiagnosticEmitter diagEmitter(unit.sm);
    lexer::Lexer lexer(unit.fileId, source, &diagEmitter);
    parser::Parser parser(lexer, &diagEmitter);
    unit.ast = parser.parseModule(modName);

    AstLowerer lowerer(&diagEmitter);
    unit.irModule = lowerer.lower_module(unit.ast);
    return unit;
}

} // namespace

TEST_CASE("AstLowerer preserves exact SourceSpan on all emitted IR instructions", "[ir][line_mapping]") {
    std::string src = 
        "fn calculate(a: int, b: int): int {\n"   // line 1
        "    let sum: int = a + b;\n"             // line 2
        "    return sum;\n"                       // line 3
        "}\n";                                    // line 4

    auto unit = lowerModuleWithSpans("src/calc.keet", src, "calc");
    REQUIRE(unit.irModule != nullptr);

    const auto& functions = unit.irModule->get_functions();
    REQUIRE(functions.size() == 1);

    const auto* fn = functions[0].get();
    REQUIRE(fn != nullptr);

    // Verify all instructions have a valid span and correct line
    bool hasLine2 = false;
    bool hasLine3 = false;

    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            REQUIRE(inst != nullptr);
            SourceSpan span = inst->get_span();
            REQUIRE(span.isValid());
            REQUIRE(span.start.fileId == unit.fileId);

            if (span.start.line == 2) hasLine2 = true;
            if (span.start.line == 3) hasLine3 = true;
        }
    }

    REQUIRE(hasLine2);
    REQUIRE(hasLine3);
}

TEST_CASE("translate_ir_function emits #line directives for IR instructions", "[emitter][ir][line_mapping]") {
    std::string src = 
        "fn testMath(x: int): int {\n"            // line 1
        "    let y = x * 2;\n"                    // line 2
        "    let z = y + 10;\n"                   // line 3
        "    return z;\n"                         // line 4
        "}\n";

    auto unit = lowerModuleWithSpans("src/math.keet", src, "math");
    REQUIRE(unit.irModule != nullptr);

    const auto* fn = unit.irModule->get_functions()[0].get();
    REQUIRE(fn != nullptr);

    CppEmitterOptions opts;
    opts.source_manager = &unit.sm;
    opts.emit_line_directives = true;

    CppEmitter emitter(opts);
    emitter.set_source_manager(&unit.sm);

    std::string cpp = emitter.translate_ir_function(*fn);

    REQUIRE(cpp.find("#line 2 \"src/math.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 3 \"src/math.keet\"\n") != std::string::npos);
    REQUIRE(cpp.find("#line 4 \"src/math.keet\"\n") != std::string::npos);
}

TEST_CASE("CppEmitter emitting ir::Module produces #line directives and respects disable flag", "[emitter][ir][line_mapping]") {
    std::string src = 
        "fn run(): int {\n"                       // line 1
        "    let val = 99;\n"                     // line 2
        "    return val;\n"                       // line 3
        "}\n";

    auto unit = lowerModuleWithSpans("src/run.keet", src, "run");
    REQUIRE(unit.irModule != nullptr);

    SECTION("When emit_line_directives is enabled") {
        CppEmitterOptions opts;
        opts.source_manager = &unit.sm;
        opts.emit_line_directives = true;

        CppEmitter emitter(*unit.irModule, opts);
        std::string cpp = emitter.emit();

        REQUIRE(cpp.find("#line 2 \"src/run.keet\"\n") != std::string::npos);
        REQUIRE(cpp.find("#line 3 \"src/run.keet\"\n") != std::string::npos);
    }

    SECTION("When emit_line_directives is disabled") {
        CppEmitterOptions opts;
        opts.source_manager = &unit.sm;
        opts.emit_line_directives = false;

        CppEmitter emitter(*unit.irModule, opts);
        std::string cpp = emitter.emit();

        REQUIRE(cpp.find("#line") == std::string::npos);
    }
}
