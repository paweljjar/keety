#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/diagnostic_translator.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/diag/source_map.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;
using namespace keety::diag;
using namespace keety::emitter;

TEST_CASE("SourceMap: Basic Mapping and Lookup", "[diag][sourcemap]") {
    SourceMap map;
    REQUIRE(map.empty());
    REQUIRE(map.size() == 0);

    // Register line 10 of C++ to line 5 of main.keet
    map.addMapping(10, 1, "src/main.keet", 5, 4);
    // Register line 20 of C++ to line 12 of main.keet
    map.addMapping(20, 1, "src/main.keet", 12, 1);

    REQUIRE_FALSE(map.empty());
    REQUIRE(map.size() == 2);

    SECTION("Exact generated line match") {
        auto loc = map.findSourceLocation(10, 5);
        REQUIRE(loc.has_value());
        REQUIRE(loc->sourceFile == "src/main.keet");
        REQUIRE(loc->sourceLine == 5);
        REQUIRE(loc->sourceColumn == 4);
    }

    SECTION("Interpolated generated line match") {
        // Line 12 is between 10 and 20 -> offset +2
        auto loc = map.findSourceLocation(12, 8);
        REQUIRE(loc.has_value());
        REQUIRE(loc->sourceFile == "src/main.keet");
        REQUIRE(loc->sourceLine == 7); // 5 + 2
    }

    SECTION("Before first mapped line returns nullopt") {
        auto loc = map.findSourceLocation(5, 1);
        REQUIRE_FALSE(loc.has_value());
    }

    SECTION("Find entry for source line") {
        auto entry = map.findEntryForSourceLine("src/main.keet", 5);
        REQUIRE(entry.has_value());
        REQUIRE(entry->generatedLine == 10);

        auto entry_offset = map.findEntryForSourceLine("src/main.keet", 7);
        REQUIRE(entry_offset.has_value());
        REQUIRE(entry_offset->generatedLine == 12);
    }
}

TEST_CASE("SourceMap: Parse From Generated C++", "[diag][sourcemap][parser]") {
    std::string cpp_source =
        "// Generated preamble\n"
        "#include <iostream>\n"
        "#line 10 \"src/app.keet\"\n"
        "void calculate() {\n"
        "    int64_t x = 42;\n"
        "}\n"
        "#line 25 \"src/app.keet\"\n"
        "void print_result() {\n"
        "    std::cout << 42;\n"
        "}\n";

    SourceMap map = SourceMap::parseFromCpp(cpp_source);
    REQUIRE(map.size() >= 5);

    // Line 4 in C++ is "void calculate() {" -> maps to app.keet line 10
    auto loc1 = map.findSourceLocation(4);
    REQUIRE(loc1.has_value());
    REQUIRE(loc1->sourceFile == "src/app.keet");
    REQUIRE(loc1->sourceLine == 10);

    // Line 5 in C++ is "    int64_t x = 42;" -> maps to app.keet line 11
    auto loc2 = map.findSourceLocation(5);
    REQUIRE(loc2.has_value());
    REQUIRE(loc2->sourceFile == "src/app.keet");
    REQUIRE(loc2->sourceLine == 11);

    // Line 8 in C++ is "void print_result() {" -> maps to app.keet line 25
    auto loc3 = map.findSourceLocation(8);
    REQUIRE(loc3.has_value());
    REQUIRE(loc3->sourceFile == "src/app.keet");
    REQUIRE(loc3->sourceLine == 25);
}

TEST_CASE("CppEmitter: SourceMap Generation in Both Modes", "[emitter][sourcemap]") {
    SourceManager sm;
    std::string code =
        "fn calculate(a: int) -> int {\n"
        "    let x = a + 1;\n"
        "    return x;\n"
        "}\n";
    FileId fid = sm.addBuffer("src/calc.keet", code);

    lexer::Lexer lexer(fid, sm.getBuffer(fid)->getContent());
    parser::Parser parser(lexer);
    auto ast = parser.parseModule();

    SECTION("With emit_line_directives = true") {
        CppEmitterOptions opts;
        opts.emit_line_directives = true;
        opts.source_manager = &sm;
        opts.source_file_path = "src/calc.keet";

        CppEmitter emitter(ast, opts);
        std::string emitted = emitter.emit();

        // Emitted code contains #line
        REQUIRE(emitted.find("#line") != std::string::npos);
        // Does not contain internal marker
        REQUIRE(emitted.find("__KSM__") == std::string::npos);

        const auto& map = emitter.get_source_map();
        REQUIRE_FALSE(map.empty());

        auto entry = map.findEntryForSourceLine("src/calc.keet", 2);
        REQUIRE(entry.has_value());
        REQUIRE(entry->sourceFile == "src/calc.keet");
        REQUIRE(entry->sourceLine == 2);
    }

    SECTION("With emit_line_directives = false (source map fallback)") {
        CppEmitterOptions opts;
        opts.emit_line_directives = false;
        opts.source_manager = &sm;
        opts.source_file_path = "src/calc.keet";

        CppEmitter emitter(ast, opts);
        std::string emitted = emitter.emit();

        // Emitted code does NOT contain #line
        REQUIRE(emitted.find("#line") == std::string::npos);
        // Emitted code does NOT contain internal marker
        REQUIRE(emitted.find("__KSM__") == std::string::npos);

        const auto& map = emitter.get_source_map();
        REQUIRE_FALSE(map.empty());

        auto entry = map.findEntryForSourceLine("src/calc.keet", 2);
        REQUIRE(entry.has_value());
        REQUIRE(entry->sourceFile == "src/calc.keet");
        REQUIRE(entry->sourceLine == 2);
        REQUIRE(entry->generatedLine > 0);
    }
}

TEST_CASE("DiagnosticTranslator: Source Mapping & Span Reconstruction", "[diag][translator][sourcemap]") {
    SourceManager sm;
    std::string source_content =
        "fn calculate() -> int {\n"
        "    let score = \"high\";\n"
        "    let total = score + 42;\n"
        "    return total;\n"
        "}\n";
    FileId fid = sm.addBuffer("src/main.keet", source_content);

    SECTION("Reconstructs span and squiggles with #line directive enabled") {
        std::string compiler_err =
            "src/main.keet:3:17: error: cannot convert 'std::__1::basic_string<char>' to 'int64_t'\n"
            "src/main.keet:3:17: note: candidate function not viable\n";

        auto diags = DiagnosticTranslator::translate(compiler_err, driver::CompilerKind::Clang, &sm, nullptr);
        REQUIRE(diags.size() == 1);
        const auto& d = diags[0];

        REQUIRE(d.code == "E3003");
        REQUIRE(d.severity == DiagnosticSeverity::Error);
        REQUIRE(d.message.find("type mismatch: cannot convert 'string' to 'int'") != std::string::npos);
        REQUIRE(d.location.line == 3);
        REQUIRE(d.location.column == 17);
        REQUIRE_FALSE(d.labels.empty());

        // Verify squiggle formatting in DiagnosticEmitter
        DiagnosticEmitter emitter(sm, ColorMode::Never);
        std::string formatted = emitter.format(d);

        REQUIRE(formatted.find("error[E3003]") != std::string::npos);
        REQUIRE(formatted.find("--> src/main.keet:3:17") != std::string::npos);
        REQUIRE(formatted.find("let total = score + 42;") != std::string::npos);
        REQUIRE(formatted.find("^^^^^") != std::string::npos);
    }

    SECTION("Remaps from C++ location when #line directives are disabled (fallback)") {
        SourceMap map;
        // Generated C++ line 45 corresponds to src/main.keet line 3
        map.addMapping(45, 1, "src/main.keet", 3, 17);

        std::string compiler_err =
            "build/.keety_build/main.cpp:45:20: error: cannot convert 'std::string' to 'int64_t'\n";

        auto diags = DiagnosticTranslator::translate(compiler_err, driver::CompilerKind::Clang, &sm, &map);
        REQUIRE(diags.size() == 1);
        const auto& d = diags[0];

        REQUIRE(d.code == "E3003");
        REQUIRE(d.location.fileId == fid);
        REQUIRE(d.location.line == 3);
        REQUIRE_FALSE(d.labels.empty());

        DiagnosticEmitter emitter(sm, ColorMode::Never);
        std::string formatted = emitter.format(d);

        // Remapped from main.cpp back to src/main.keet!
        REQUIRE(formatted.find("--> src/main.keet:3:") != std::string::npos);
        REQUIRE(formatted.find("let total = score + 42;") != std::string::npos);
        REQUIRE(formatted.find("^^^^^") != std::string::npos);
    }

    SECTION("Column offset recovery for symbol mentioned in error message") {
        SourceManager sm2;
        std::string cls_code =
            "class Player {\n"
            "    name: string;\n"
            "    health: int;\n"
            "}\n"
            "fn get_hp(p: Player) -> int {\n"
            "    return p.hp;\n"
            "}\n";
        sm2.addBuffer("src/player.keet", cls_code);

        // Column is 0 or 1 because compiler stripped it
        std::string compiler_err =
            "src/player.keet:6:1: error: no member named 'hp' in 'Player'\n"
            "src/player.keet:6:1: note: did you mean 'health'?\n";

        auto diags = DiagnosticTranslator::translate(compiler_err, driver::CompilerKind::Clang, &sm2, nullptr);
        REQUIRE(diags.size() == 1);
        const auto& d = diags[0];

        REQUIRE(d.code == "E3011");
        REQUIRE(d.location.line == 6);
        // Snapped directly to 'hp' at column 14
        REQUIRE(d.location.column == 14);
        REQUIRE_FALSE(d.labels.empty());

        DiagnosticEmitter emitter(sm2, ColorMode::Never);
        std::string formatted = emitter.format(d);

        REQUIRE(formatted.find("error[E3011]") != std::string::npos);
        REQUIRE(formatted.find("--> src/player.keet:6:14") != std::string::npos);
        REQUIRE(formatted.find("return p.hp;") != std::string::npos);
        REQUIRE(formatted.find("^^") != std::string::npos);
        REQUIRE(formatted.find("= help: did you mean 'health'?") != std::string::npos);
    }

    SECTION("Unclassified backend error maps to E7001 with span") {
        std::string compiler_err =
            "src/main.keet:2:5: error: platform-specific SIMD instruction requires AVX512 support\n";

        auto diags = DiagnosticTranslator::translate(compiler_err, driver::CompilerKind::Clang, &sm, nullptr);
        REQUIRE(diags.size() == 1);
        const auto& d = diags[0];

        REQUIRE(d.code == "E7001");
        REQUIRE(d.location.line == 2);
        REQUIRE_FALSE(d.labels.empty());

        DiagnosticEmitter emitter(sm, ColorMode::Never);
        std::string formatted = emitter.format(d);
        REQUIRE(formatted.find("error[E7001]") != std::string::npos);
        REQUIRE(formatted.find("let score = \"high\";") != std::string::npos);
    }
}
