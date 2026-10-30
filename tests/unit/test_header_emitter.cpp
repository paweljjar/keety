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

TEST_CASE("C++ Header Emitter maps KeetScript types to C++17 types", "[emitter][types]") {
    using namespace keety::ast;
    keety::diag::SourceSpan dummySpan{};

    SECTION("Primitive types") {
        PrimitiveType tInt(dummySpan, PrimitiveTypeKind::Int);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tInt) == "int64_t");

        PrimitiveType tFloat(dummySpan, PrimitiveTypeKind::Float);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tFloat) == "double");

        PrimitiveType tBool(dummySpan, PrimitiveTypeKind::Bool);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tBool) == "bool");

        PrimitiveType tStr(dummySpan, PrimitiveTypeKind::String);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tStr) == "std::string");

        PrimitiveType tChar(dummySpan, PrimitiveTypeKind::Char);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tChar) == "char");

        PrimitiveType tVoid(dummySpan, PrimitiveTypeKind::Void);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tVoid) == "void");

        PrimitiveType tNull(dummySpan, PrimitiveTypeKind::Null);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tNull) == "std::monostate");

        PrimitiveType tErr(dummySpan, PrimitiveTypeKind::Error);
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&tErr) == "keet::error");
    }

    SECTION("Collection types") {
        auto elem = std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Int);
        ArrayType arr(dummySpan, std::move(elem));
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&arr) == "std::vector<int64_t>");

        auto k = std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::String);
        auto v = std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Float);
        MapType mp(dummySpan, std::move(k), std::move(v));
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&mp) == "std::unordered_map<std::string, double>");
    }

    SECTION("Union types") {
        std::vector<std::unique_ptr<TypeAnnotation>> vars;
        vars.push_back(std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::String));
        vars.push_back(std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Null));
        UnionType un(dummySpan, std::move(vars));
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&un) == "std::variant<std::string, std::monostate>");
    }

    SECTION("Function types") {
        std::vector<FuncTypeParam> params;
        params.push_back(FuncTypeParam{"a", std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Int), false});
        params.push_back(FuncTypeParam{"b", std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Int), true});
        auto ret = std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Bool);
        FunctionType fn(dummySpan, std::move(params), std::move(ret));
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&fn) == "std::function<bool(int64_t, int64_t&)>");
    }

    SECTION("Custom and qualified types") {
        CustomType simple(dummySpan, "Player");
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&simple) == "Player");

        std::vector<std::unique_ptr<TypeAnnotation>> args;
        args.push_back(std::make_unique<PrimitiveType>(dummySpan, PrimitiveTypeKind::Int));
        CustomType generic(dummySpan, "Box", std::move(args));
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&generic) == "Box<int64_t>");

        CustomType qual(dummySpan, "Vector", {}, "math");
        REQUIRE(keety::emitter::HeaderEmitter::to_cpp_type(&qual) == "keet_module_math::Vector");
    }
}

TEST_CASE("C++ Header Emitter emits forward declarations, constexpr constants, enums, classes, and error types", "[emitter][declarations]") {
    const std::string source = R"(
        define MAX_BUFFER_SIZE: int = 4096;
        define PI: float = 3.14159;
        define VERSION: string = "1.0.0";

        enum StatusCode: int {
            OK = 200,
            NotFound = 404,
            InternalError = 500
        }

        errtype TimeoutError {
            pub let timeoutMs: int;
        }

        class Entity {
            pub let id: int = 1;
            pub let active: bool = true;

            fn isAlive(): bool {
                return true;
            }
        }

        class Player inherits Entity {
            pub let name: string;
            pub let scores: array<int>;

            fn addScore(pts: int, mut buffer: array<int>): void {
            }
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "game_engine");
    keety::emitter::HeaderEmitter emitter(ast);

    std::string header = emitter.emit();

    SECTION("Emits forward declarations for all classes, error types, and enums") {
        REQUIRE(header.find("// --- Forward Declarations ---") != std::string::npos);
        REQUIRE(header.find("struct Entity;") != std::string::npos);
        REQUIRE(header.find("struct Player;") != std::string::npos);
        REQUIRE(header.find("struct TimeoutError;") != std::string::npos);
        REQUIRE(header.find("enum class StatusCode : int64_t;") != std::string::npos);
    }

    SECTION("Emits compile-time constexpr constant definitions") {
        REQUIRE(header.find("// --- Compile-Time Constants ---") != std::string::npos);
        REQUIRE(header.find("inline constexpr int64_t MAX_BUFFER_SIZE = 4096;") != std::string::npos);
        REQUIRE(header.find("inline constexpr double PI = 3.14159;") != std::string::npos);
        REQUIRE(header.find("inline constexpr std::string VERSION = \"1.0.0\";") != std::string::npos);
    }

    SECTION("Emits enum definitions with underlying type and enumerators") {
        REQUIRE(header.find("// --- Enums ---") != std::string::npos);
        REQUIRE(header.find("enum class StatusCode : int64_t {") != std::string::npos);
        REQUIRE(header.find("OK = 200,") != std::string::npos);
        REQUIRE(header.find("NotFound = 404,") != std::string::npos);
        REQUIRE(header.find("InternalError = 500") != std::string::npos);
    }

    SECTION("Emits error type definitions deriving from keet::error with custom fields and constructor") {
        REQUIRE(header.find("// --- Error Types ---") != std::string::npos);
        REQUIRE(header.find("struct TimeoutError : public keet::error {") != std::string::npos);
        REQUIRE(header.find("int64_t timeoutMs{};") != std::string::npos);
        REQUIRE(header.find("TimeoutError(int64_t timeoutMs = {}, std::string msg = \"TimeoutError\")") != std::string::npos);
        REQUIRE(header.find(": keet::error(\"TimeoutError\", std::move(msg)), timeoutMs(timeoutMs)") != std::string::npos);
    }

    SECTION("Emits class definitions with base inheritance, fields, default constructor, and methods") {
        REQUIRE(header.find("// --- Classes and Structs ---") != std::string::npos);

        // Entity base class
        REQUIRE(header.find("struct Entity {") != std::string::npos);
        REQUIRE(header.find("int64_t id = 1;") != std::string::npos);
        REQUIRE(header.find("bool active = true;") != std::string::npos);
        REQUIRE(header.find("Entity() = default;") != std::string::npos);
        REQUIRE(header.find("bool isAlive();") != std::string::npos);

        // Player derived class
        REQUIRE(header.find("struct Player : public Entity {") != std::string::npos);
        REQUIRE(header.find("std::string name{};") != std::string::npos);
        REQUIRE(header.find("std::vector<int64_t> scores{};") != std::string::npos);
        REQUIRE(header.find("Player() = default;") != std::string::npos);
        REQUIRE(header.find("void addScore(int64_t pts, std::vector<int64_t>& buffer);") != std::string::npos);
    }
}

TEST_CASE("C++ Header Emitter emits const-views for exported module variables", "[emitter][const_views]") {
    const std::string source = R"(
        export let activeRequests: int = 0;
        export let serverConfig: string = "production";
        let internalSecret: int = 1337;
    )";

    keety::parser::ModuleAST ast = parseSource(source, "telemetry");
    keety::emitter::HeaderEmitter emitter(ast);

    std::string header = emitter.emit();

    SECTION("Emits extern storage and inline const-view reference for exported variables") {
        REQUIRE(header.find("// --- Exported Variable Const-View Accessor ---") != std::string::npos);
        REQUIRE(header.find("extern int64_t _internal_activeRequests;\n    inline const int64_t& activeRequests = _internal_activeRequests;") != std::string::npos);
        REQUIRE(header.find("extern std::string _internal_serverConfig;\n    inline const std::string& serverConfig = _internal_serverConfig;") != std::string::npos);
    }

    SECTION("Does not emit const-views for unexported private module variables") {
        REQUIRE(header.find("internalSecret") == std::string::npos);
        REQUIRE(header.find("_internal_internalSecret") == std::string::npos);
    }
}

TEST_CASE("C++ Header Emitter emits generic function templates and function prototypes", "[emitter][functions]") {
    const std::string source = R"(
        fn identity<T>(x: T): T {
            return x;
        }

        fn pair<T, U>(first: T, second: U): T {
            return first;
        }

        fn recordHit(path: string): void {
        }

        fn computeRatio(hits: int, total: int): float {
            return 0.0;
        }

        fn modifyBuffer(mut buffer: array<int>, tag: string): void {
        }
    )";

    keety::parser::ModuleAST ast = parseSource(source, "metrics");
    keety::emitter::HeaderEmitter emitter(ast);

    std::string header = emitter.emit();

    SECTION("Emits generic function templates with typename parameters and const-ref formatting") {
        REQUIRE(header.find("// --- Generic Function Templates ---") != std::string::npos);
        REQUIRE(header.find("template <typename T>\n    T identity(const T& x);") != std::string::npos);
        REQUIRE(header.find("template <typename T, typename U>\n    T pair(const T& first, const U& second);") != std::string::npos);
    }

    SECTION("Emits concrete function prototypes with canonical parameter conventions") {
        REQUIRE(header.find("// --- Exported Function Prototypes ---") != std::string::npos);
        REQUIRE(header.find("void recordHit(const std::string& path);") != std::string::npos);
        REQUIRE(header.find("double computeRatio(int64_t hits, int64_t total);") != std::string::npos);
        REQUIRE(header.find("void modifyBuffer(std::vector<int64_t>& buffer, const std::string& tag);") != std::string::npos);
    }
}

TEST_CASE("C++ Header Emitter processes IR Module globals and functions", "[emitter][ir]") {
    using namespace keety::typechecker;
    keety::ir::Module irModule("network_runtime");

    // Add exported global
    keety::ir::GlobalVariable exportedVar;
    exportedVar.name = "active_connections";
    exportedVar.type = std::make_shared<Type>(TypeKind::Int);
    exportedVar.is_export = true;
    irModule.add_global(std::move(exportedVar));

    // Add unexported global
    keety::ir::GlobalVariable internalVar;
    internalVar.name = "internal_cache_size";
    internalVar.type = std::make_shared<Type>(TypeKind::Int);
    internalVar.is_export = false;
    irModule.add_global(std::move(internalVar));

    // Add function
    auto fn = std::make_unique<keety::ir::Function>("handle_request", std::make_shared<Type>(TypeKind::Bool));
    fn->add_parameter(keety::ir::FunctionParameter{"endpoint", std::make_shared<Type>(TypeKind::String), false, true, false});
    fn->add_parameter(keety::ir::FunctionParameter{"retries", std::make_shared<Type>(TypeKind::Int), true, false, false});
    irModule.add_function(std::move(fn));

    keety::emitter::HeaderEmitter emitter(irModule);
    std::string header = emitter.emit();

    SECTION("Emits const-views for exported IR globals only") {
        REQUIRE(header.find("// --- Exported Variable Const-View Accessor ---") != std::string::npos);
        REQUIRE(header.find("extern int64_t _internal_active_connections;\n    inline const int64_t& active_connections = _internal_active_connections;") != std::string::npos);
        REQUIRE(header.find("internal_cache_size") == std::string::npos);
    }

    SECTION("Emits function prototypes for IR functions") {
        REQUIRE(header.find("// --- Exported Function Prototypes ---") != std::string::npos);
        REQUIRE(header.find("bool handle_request(const std::string& endpoint, int64_t& retries);") != std::string::npos);
    }
}

