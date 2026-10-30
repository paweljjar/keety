#include <catch2/catch_test_macros.hpp>

#include <keety/ast/decl.hpp>
#include <keety/emitter/cpp_emitter.hpp>
#include <keety/ir/module.hpp>
#include <keety/ir/operand.hpp>
#include <keety/parser/parser.hpp>
#include <keety/typechecker/type.hpp>

#include <sstream>
#include <string>

namespace {

keety::parser::ModuleAST parseSource(const std::string& source, const std::string& moduleName = "metrics") {
    keety::diag::SourceManager sm;
    keety::diag::DiagnosticEmitter emitter(sm);
    keety::lexer::Lexer lexer(1, source, &emitter);
    keety::parser::Parser parser(lexer, &emitter);
    return parser.parseModule(moduleName);
}

} // namespace

TEST_CASE("C++ Implementation Emitter produces canonical preamble, header includes, and namespace", "[emitter][cpp]") {
    keety::emitter::CppEmitter emitter;
    emitter.set_module_name("metrics");

    std::string cpp = emitter.emit();

    SECTION("Emits compiler banner and standard headers") {
        REQUIRE(cpp.find("// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.") != std::string::npos);
        REQUIRE(cpp.find("#include \"metrics.hpp\"") != std::string::npos);
        REQUIRE(cpp.find("#include <iostream>") != std::string::npos);
    }

    SECTION("Encapsulates within module namespace") {
        REQUIRE(cpp.find("namespace keet_module_metrics {\n") != std::string::npos);
        REQUIRE(cpp.find("} // namespace keet_module_metrics\n") != std::string::npos);
    }

    SECTION("Supports stream-based emission") {
        std::ostringstream oss;
        emitter.emit(oss);
        REQUIRE(oss.str() == cpp);
    }
}

TEST_CASE("C++ Implementation Emitter supports custom include path and custom namespace", "[emitter][cpp]") {
    keety::emitter::CppEmitterOptions options;
    options.header_include = "\"custom/path/metrics_api.hpp\"";
    options.custom_namespace = "telemetry::backend";
    options.standard_headers = {"<iostream>", "<cassert>"};

    keety::emitter::CppEmitter emitter(options);
    emitter.set_module_name("metrics");

    std::string cpp = emitter.emit();

    REQUIRE(cpp.find("#include \"custom/path/metrics_api.hpp\"") != std::string::npos);
    REQUIRE(cpp.find("#include <iostream>") != std::string::npos);
    REQUIRE(cpp.find("#include <cassert>") != std::string::npos);
    REQUIRE(cpp.find("namespace telemetry::backend {\n") != std::string::npos);
    REQUIRE(cpp.find("} // namespace telemetry::backend\n") != std::string::npos);
}

TEST_CASE("C++ Implementation Emitter emits module variable storage allocations with internal prefix", "[emitter][storage]") {
    const std::string source = R"(
        export let activeRequests: int = 0;
        export let serverConfig: string = "production";
        let internalCounter: int = 42;
        let uninitBuffer: array<int>;
        const MAX_CONNECTIONS: int = 1000;
    )";

    keety::parser::ModuleAST ast = parseSource(source, "metrics");
    keety::emitter::CppEmitter emitter(ast);

    std::string cpp = emitter.emit();

    SECTION("Allocates storage with _internal_ prefix for both exported and internal variables") {
        REQUIRE(cpp.find("// Storage definitions for module-level variables") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_activeRequests = 0;") != std::string::npos);
        REQUIRE(cpp.find("std::string _internal_serverConfig = \"production\";") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_internalCounter = 42;") != std::string::npos);
        REQUIRE(cpp.find("std::vector<int64_t> _internal_uninitBuffer = {};") != std::string::npos);
        REQUIRE(cpp.find("const int64_t _internal_MAX_CONNECTIONS = 1000;") != std::string::npos);
    }

    SECTION("Ensures internal prefix helper prevents double prefixing") {
        REQUIRE(keety::emitter::CppEmitter::make_internal_var_name("requests") == "_internal_requests");
        REQUIRE(keety::emitter::CppEmitter::make_internal_var_name("_internal_requests") == "_internal_requests");
        REQUIRE(keety::emitter::CppEmitter::make_internal_var_name("") == "_internal_");
    }
}

TEST_CASE("C++ Implementation Emitter ingests globals from KeetIR Module", "[emitter][ir]") {
    using namespace keety::typechecker;
    keety::ir::Module irModule("gateway");

    // 1. Exported int global with constant operand initializer
    keety::ir::GlobalVariable g1;
    g1.name = "active_clients";
    g1.type = std::make_shared<Type>(TypeKind::Int);
    g1.is_export = true;
    g1.initial_value = keety::ir::Operand::create_int(10);
    irModule.add_global(std::move(g1));

    // 2. String global with string literal operand
    keety::ir::GlobalVariable g2;
    g2.name = "endpoint_url";
    g2.type = std::make_shared<Type>(TypeKind::String);
    g2.is_export = false;
    g2.initial_value = keety::ir::Operand::create_string("https://api.example.com");
    irModule.add_global(std::move(g2));

    // 3. Float constant global
    keety::ir::GlobalVariable g3;
    g3.name = "sample_rate";
    g3.type = std::make_shared<Type>(TypeKind::Float);
    g3.is_const = true;
    g3.is_export = true;
    g3.initial_value = keety::ir::Operand::create_float(0.75);
    irModule.add_global(std::move(g3));

    // 4. Uninitialized bool global
    keety::ir::GlobalVariable g4;
    g4.name = "is_ready";
    g4.type = std::make_shared<Type>(TypeKind::Bool);
    g4.is_export = false;
    g4.initial_value = std::nullopt;
    irModule.add_global(std::move(g4));

    keety::emitter::CppEmitter emitter(irModule);
    std::string cpp = emitter.emit();

    SECTION("Emits storage definitions matching IR global types and constant operands") {
        REQUIRE(cpp.find("// Storage definitions for module-level variables") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_active_clients = 10;") != std::string::npos);
        REQUIRE(cpp.find("std::string _internal_endpoint_url = \"https://api.example.com\";") != std::string::npos);
        REQUIRE(cpp.find("const double _internal_sample_rate = 0.75;") != std::string::npos);
        REQUIRE(cpp.find("bool _internal_is_ready = {};") != std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter handles single vs multiple storage allocation comments", "[emitter][comments]") {
    SECTION("Single variable emits singular comment") {
        keety::emitter::CppEmitter emitter;
        emitter.set_module_name("single_mod");
        emitter.add_storage_allocation("counter", "int64_t", "0");

        std::string cpp = emitter.emit();
        REQUIRE(cpp.find("// Storage definition for module-level variable\n") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_counter = 0;") != std::string::npos);
    }

    SECTION("Multiple variables emit plural comment") {
        keety::emitter::CppEmitter emitter;
        emitter.set_module_name("multi_mod");
        emitter.add_storage_allocation("v1", "int64_t", "1");
        emitter.add_storage_allocation("v2", "int64_t", "2");

        std::string cpp = emitter.emit();
        REQUIRE(cpp.find("// Storage definitions for module-level variables\n") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_v1 = 1;") != std::string::npos);
        REQUIRE(cpp.find("int64_t _internal_v2 = 2;") != std::string::npos);
    }

    SECTION("No variables emit no storage section") {
        keety::emitter::CppEmitter emitter;
        emitter.set_module_name("empty_mod");

        std::string cpp = emitter.emit();
        REQUIRE(cpp.find("// Storage definition") == std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter translates function bodies to C++17 with containers", "[emitter][functions]") {
    const std::string source = R"(
        fn buildBuffer(initial: int): array<int> {
            let buf: array<int> = [1, 2, 3];
            buf.push(initial);
            return buf;
        }

        fn lookupScore(scores: map<string, int>, key: string): int {
            return scores[key];
        }

        fn parsePayload(val: int | string): string {
            return val as string;
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "data_pipeline");
    keety::emitter::CppEmitter emitter(ast);

    std::string cpp = emitter.emit();

    SECTION("Translates array functions using std::vector and push_back") {
        REQUIRE(cpp.find("std::vector<int64_t> buildBuffer(int64_t initial) {") != std::string::npos);
        REQUIRE(cpp.find("std::vector<int64_t> buf = {1, 2, 3};") != std::string::npos);
        REQUIRE(cpp.find("buf.push_back(initial);") != std::string::npos);
        REQUIRE(cpp.find("return buf;") != std::string::npos);
    }

    SECTION("Translates map functions using std::unordered_map indexing") {
        REQUIRE(cpp.find("int64_t lookupScore(const std::unordered_map<std::string, int64_t>& scores, const std::string& key) {") != std::string::npos);
        REQUIRE(cpp.find("return scores[key];") != std::string::npos);
    }

    SECTION("Translates union functions using std::variant and std::get") {
        REQUIRE(cpp.find("std::string parsePayload(const std::variant<int64_t, std::string>& val) {") != std::string::npos);
        REQUIRE(cpp.find("return std::get<std::string>(val);") != std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter translates functions accessing module-level mutable variables", "[emitter][functions]") {
    const std::string source = R"(
        export let activeRequests: int = 0;

        fn recordHit(path: string): void {
            activeRequests += 1;
        }

        fn computeRatio(hits: int, total: int): int {
            if (total == 0) {
                zonk "Cannot divide by zero in computeRatio";
            }
            return hits / total;
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "metrics");
    keety::emitter::CppEmitter emitter(ast);

    std::string cpp = emitter.emit();

    SECTION("Module variable references within functions map to _internal_ storage") {
        REQUIRE(cpp.find("int64_t _internal_activeRequests = 0;") != std::string::npos);
        REQUIRE(cpp.find("void recordHit(const std::string& path) {") != std::string::npos);
        REQUIRE(cpp.find("_internal_activeRequests += 1;") != std::string::npos);
    }

    SECTION("Conditional branches and zonk statements map to C++ if and throw keet::error") {
        REQUIRE(cpp.find("int64_t computeRatio(int64_t hits, int64_t total) {") != std::string::npos);
        REQUIRE(cpp.find("if (total == 0) {") != std::string::npos);
        REQUIRE(cpp.find("throw keet::error(\"Error\", \"Cannot divide by zero in computeRatio\");") != std::string::npos);
        REQUIRE(cpp.find("return hits / total;") != std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter translates class method definitions", "[emitter][methods]") {
    const std::string source = R"(
        class Counter {
            pub let value: int = 0;

            fn increment(): void {
                this.value += 1;
            }
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "domain");
    keety::emitter::CppEmitter emitter(ast);

    std::string cpp = emitter.emit();

    SECTION("Emits qualified class method definitions with this-> pointer access") {
        REQUIRE(cpp.find("void Counter::increment() {") != std::string::npos);
        REQUIRE(cpp.find("this->value += 1;") != std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter translates for-in collection loops and structured bindings", "[emitter][loops]") {
    const std::string source = R"(
        fn sumEntries(scores: map<string, int>): int {
            let total: int = 0;
            for (const k, v in scores) {
                total += v;
            }
            return total;
        }

        fn sumItems(items: array<int>): int {
            let total: int = 0;
            for (const item in items) {
                total += item;
            }
            return total;
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "algorithms");
    keety::emitter::CppEmitter emitter(ast);

    std::string cpp = emitter.emit();

    SECTION("Translates map iteration using C++17 structured bindings") {
        REQUIRE(cpp.find("for (const auto& [k, v] : scores) {") != std::string::npos);
        REQUIRE(cpp.find("total += v;") != std::string::npos);
    }

    SECTION("Translates array iteration using range-for") {
        REQUIRE(cpp.find("for (const auto& item : items) {") != std::string::npos);
        REQUIRE(cpp.find("total += item;") != std::string::npos);
    }
}

TEST_CASE("C++ Implementation Emitter translates IR functions and instructions", "[emitter][ir_fn]") {
    using namespace keety::typechecker;
    keety::ir::Module irModule("ir_compute");

    auto fn = std::make_unique<keety::ir::Function>("add_numbers", std::make_shared<Type>(TypeKind::Int));
    fn->add_parameter(keety::ir::FunctionParameter{"a", std::make_shared<Type>(TypeKind::Int), false, false, false});
    fn->add_parameter(keety::ir::FunctionParameter{"b", std::make_shared<Type>(TypeKind::Int), false, false, false});

    auto* entry = fn->create_basic_block("entry");
    auto res_temp = fn->create_temp(std::make_shared<Type>(TypeKind::Int));
    entry->append_instruction(std::make_unique<keety::ir::BinaryInst>(
        keety::ir::BinaryOpKind::Add,
        res_temp,
        keety::ir::Operand::create_var("a", std::make_shared<Type>(TypeKind::Int)),
        keety::ir::Operand::create_var("b", std::make_shared<Type>(TypeKind::Int))
    ));
    entry->append_instruction(std::make_unique<keety::ir::ReturnInst>(res_temp));

    irModule.add_function(std::move(fn));

    keety::emitter::CppEmitter emitter(irModule);
    std::string cpp = emitter.emit();

    SECTION("Emits C++ function definition translating IR instructions") {
        REQUIRE(cpp.find("int64_t add_numbers(int64_t a, int64_t b) {") != std::string::npos);
        REQUIRE(cpp.find("auto _t0 = a + b;") != std::string::npos);
        REQUIRE(cpp.find("return _t0;") != std::string::npos);
    }
}

