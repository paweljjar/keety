#include <catch2/catch_test_macros.hpp>
#include <keety/emitter/header_emitter.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

#include <sstream>
#include <string>

namespace {

keety::parser::ModuleAST parseSource(const std::string& source, const std::string& moduleName = "test_mod") {
    keety::diag::SourceManager sm;
    keety::diag::DiagnosticEmitter emitter(sm);
    keety::lexer::Lexer lexer(1, source, &emitter);
    keety::parser::Parser parser(lexer, &emitter);
    return parser.parseModule(moduleName);
}

} // namespace

TEST_CASE("C++ Header Emitter produces standard preamble with header guard and standard includes", "[emitter][header]") {
    keety::emitter::HeaderEmitter emitter;
    std::string header = emitter.emit();

    SECTION("Emits code generator banner and pragma once guard") {
        REQUIRE(header.find("// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.") != std::string::npos);
        REQUIRE(header.find("#pragma once") != std::string::npos);
    }

    SECTION("Emits all canonical C++17 standard headers") {
        REQUIRE(header.find("#include <algorithm>") != std::string::npos);
        REQUIRE(header.find("#include <cmath>") != std::string::npos);
        REQUIRE(header.find("#include <cstdint>") != std::string::npos);
        REQUIRE(header.find("#include <exception>") != std::string::npos);
        REQUIRE(header.find("#include <functional>") != std::string::npos);
        REQUIRE(header.find("#include <memory>") != std::string::npos);
        REQUIRE(header.find("#include <string>") != std::string::npos);
        REQUIRE(header.find("#include <unordered_map>") != std::string::npos);
        REQUIRE(header.find("#include <utility>") != std::string::npos);
        REQUIRE(header.find("#include <variant>") != std::string::npos);
        REQUIRE(header.find("#include <vector>") != std::string::npos);
    }

    SECTION("Emits core runtime include") {
        REQUIRE(header.find("#include <keet/runtime.hpp>") != std::string::npos);
    }

    SECTION("Output ends with newline and has no imported headers by default") {
        REQUIRE(!header.empty());
        REQUIRE(header.back() == '\n');
        REQUIRE(emitter.get_imported_headers().empty());
    }
}

TEST_CASE("C++ Header Emitter respects header customization options", "[emitter][header]") {
    SECTION("Can disable banner and pragma once") {
        keety::emitter::HeaderEmitterOptions options;
        options.emit_banner = false;
        options.emit_pragma_once = false;

        keety::emitter::HeaderEmitter emitter(options);
        std::string header = emitter.emit();

        REQUIRE(header.find("// Generated ahead-of-time") == std::string::npos);
        REQUIRE(header.find("#pragma once") == std::string::npos);
        REQUIRE(header.find("#include <cstdint>") != std::string::npos);
    }

    SECTION("Can disable or customize runtime header") {
        keety::emitter::HeaderEmitterOptions options;
        options.emit_runtime_header = false;

        keety::emitter::HeaderEmitter emitter(options);
        std::string header = emitter.emit();
        REQUIRE(header.find("#include <keet/runtime.hpp>") == std::string::npos);

        options.emit_runtime_header = true;
        options.runtime_header_path = "<custom/runtime.hpp>";
        keety::emitter::HeaderEmitter customEmitter(options);
        std::string customHeader = customEmitter.emit();
        REQUIRE(customHeader.find("#include <custom/runtime.hpp>") != std::string::npos);
    }

    SECTION("Can append custom standard headers") {
        keety::emitter::HeaderEmitter emitter;
        emitter.add_standard_header("iostream");
        emitter.add_standard_header("<chrono>");
        // Duplicate should be ignored
        emitter.add_standard_header("<iostream>");

        std::string header = emitter.emit();
        REQUIRE(header.find("#include <iostream>") != std::string::npos);
        REQUIRE(header.find("#include <chrono>") != std::string::npos);
    }
}

TEST_CASE("C++ Header Emitter resolves imported header paths and deduplicates includes", "[emitter][header]") {
    SECTION("Path resolution converts module specifiers to C++ header includes") {
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("./math") == "\"./math.hpp\"");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("./utils/calc.keet") == "\"./utils/calc.hpp\"");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("./native/hash.h") == "\"./native/hash.h\"");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("./native/hash.hpp") == "\"./native/hash.hpp\"");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("io") == "<keet/std/io.hpp>");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("<vector>") == "<vector>");
        REQUIRE(keety::emitter::HeaderEmitter::resolve_header_include_path("\"custom.hpp\"") == "\"custom.hpp\"");
    }

    SECTION("Adding imported headers deduplicates and formats directives") {
        keety::emitter::HeaderEmitter emitter;
        emitter.add_imported_header("./math");
        emitter.add_imported_header("./math.keet"); // Resolves to same header
        emitter.add_imported_header("./utils/logger");

        std::string header = emitter.emit();
        REQUIRE(header.find("#include \"./math.hpp\"") != std::string::npos);
        REQUIRE(header.find("#include \"./utils/logger.hpp\"") != std::string::npos);

        // Ensure single occurrence of math.hpp
        size_t firstPos = header.find("#include \"./math.hpp\"");
        size_t secondPos = header.find("#include \"./math.hpp\"", firstPos + 1);
        REQUIRE(secondPos == std::string::npos);
    }

    SECTION("Supports sorting imported headers when requested") {
        keety::emitter::HeaderEmitterOptions options;
        options.sort_imported_headers = true;

        keety::emitter::HeaderEmitter emitter(options);
        emitter.add_imported_header("./z_module");
        emitter.add_imported_header("./a_module");
        emitter.add_imported_header("./m_module");

        const auto& imported = emitter.get_imported_headers();
        REQUIRE(imported.size() == 3);
        REQUIRE(imported[0] == "\"./a_module.hpp\"");
        REQUIRE(imported[1] == "\"./m_module.hpp\"");
        REQUIRE(imported[2] == "\"./z_module.hpp\"");
    }
}

TEST_CASE("C++ Header Emitter extracts imports from parsed AST module", "[emitter][header]") {
    const std::string source = R"(
        import "./math";
        import "./utils/helper.keet";
        import "./native/fast_hash.hpp";
        from "./geometry" import Point, Circle;
        import "./math"; // duplicate import

        export let count: int = 42;
    )";

    keety::parser::ModuleAST moduleAst = parseSource(source, "metrics");
    keety::emitter::HeaderEmitter emitter(moduleAst);

    REQUIRE(emitter.get_module_name() == "metrics");

    std::string header = emitter.emit();

    REQUIRE(header.find("#include \"./math.hpp\"") != std::string::npos);
    REQUIRE(header.find("#include \"./utils/helper.hpp\"") != std::string::npos);
    REQUIRE(header.find("#include \"./native/fast_hash.hpp\"") != std::string::npos);
    REQUIRE(header.find("#include \"./geometry.hpp\"") != std::string::npos);

    // Verify deduplication in emitted header
    size_t pos1 = header.find("#include \"./math.hpp\"");
    size_t pos2 = header.find("#include \"./math.hpp\"", pos1 + 1);
    REQUIRE(pos2 == std::string::npos);
}

TEST_CASE("C++ Header Emitter interacts with IR Module and supports stream emission", "[emitter][header]") {
    keety::ir::Module irModule("network_service");
    keety::emitter::HeaderEmitter emitter(irModule);

    REQUIRE(emitter.get_module_name() == "network_service");

    std::ostringstream oss;
    emitter.emit(oss);

    REQUIRE(oss.str() == emitter.emit());
    REQUIRE(oss.str().find("#pragma once") != std::string::npos);
}

TEST_CASE("C++ Header Emitter encapsulates declarations within module namespace", "[emitter][namespace]") {
    SECTION("Default module name produces keet_module_main namespace") {
        keety::emitter::HeaderEmitter emitter;
        REQUIRE(emitter.get_namespace_name() == "keet_module_main");

        std::string header = emitter.emit();
        REQUIRE(header.find("namespace keet_module_main {\n") != std::string::npos);
        REQUIRE(header.find("} // namespace keet_module_main\n") != std::string::npos);
    }

    SECTION("Named module produces keet_module_<name> namespace") {
        keety::emitter::HeaderEmitter emitter;
        emitter.set_module_name("metrics");
        REQUIRE(emitter.get_namespace_name() == "keet_module_metrics");

        std::string header = emitter.emit();
        REQUIRE(header.find("namespace keet_module_metrics {\n") != std::string::npos);
        REQUIRE(header.find("} // namespace keet_module_metrics\n") != std::string::npos);
    }

    SECTION("Sanitizes invalid identifier characters in module name") {
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("math/stats") == "keet_module_math_stats");
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("my-service") == "keet_module_my_service");
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("net.http") == "keet_module_net_http");
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("99bottles") == "keet_module__99bottles");
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("keet_module_already_prefixed") == "keet_module_already_prefixed");
        REQUIRE(keety::emitter::HeaderEmitter::make_module_namespace("") == "keet_module_main");
    }

    SECTION("Allows custom namespace override") {
        keety::emitter::HeaderEmitterOptions options;
        options.custom_namespace = "enterprise::telemetry";

        keety::emitter::HeaderEmitter emitter(options);
        REQUIRE(emitter.get_namespace_name() == "enterprise::telemetry");

        std::string header = emitter.emit();
        REQUIRE(header.find("namespace enterprise::telemetry {\n") != std::string::npos);
        REQUIRE(header.find("} // namespace enterprise::telemetry\n") != std::string::npos);
    }

    SECTION("Allows suppressing namespace encapsulation") {
        keety::emitter::HeaderEmitterOptions options;
        options.emit_namespace = false;

        keety::emitter::HeaderEmitter emitter(options);
        std::string header = emitter.emit();

        REQUIRE(header.find("namespace ") == std::string::npos);
        REQUIRE(header.find("} // namespace") == std::string::npos);
    }

    SECTION("Parsed ModuleAST sets namespace accordingly") {
        const std::string source = "let x: int = 1;";
        keety::parser::ModuleAST ast = parseSource(source, "user_account");

        keety::emitter::HeaderEmitter emitter(ast);
        REQUIRE(emitter.get_namespace_name() == "keet_module_user_account");

        std::string header = emitter.emit();
        REQUIRE(header.find("namespace keet_module_user_account {\n") != std::string::npos);
        REQUIRE(header.find("} // namespace keet_module_user_account\n") != std::string::npos);
    }
}
