#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_translator.hpp>

using namespace keety::diag;
using namespace keety::driver;

TEST_CASE("DiagnosticTranslator: C++ STL Type Demangling", "[diag][translator][demangle]") {
    SECTION("Scalar and primitive types") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("int64_t") == "int");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("long long") == "int");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("double") == "float");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("float") == "float");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("bool") == "bool");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("char") == "char");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("void") == "void");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("const int64_t&") == "int");
    }

    SECTION("String types") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::string") == "string");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::basic_string<char>") == "string");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::__1::basic_string<char, std::char_traits<char>, std::allocator<char>>") == "string");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("const std::string&") == "string");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::string_view") == "string");
    }

    SECTION("Vector / array types") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::vector<int64_t>") == "[int]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::__1::vector<int64_t, std::allocator<int64_t>>") == "[int]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::vector<std::string>") == "[string]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::vector<std::vector<int64_t>>") == "[[int]]");
    }

    SECTION("Map types") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::unordered_map<std::string, int64_t>") == "[string: int]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::__1::unordered_map<std::string, std::vector<int64_t>>") == "[string: [int]]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::unordered_map<std::string, std::variant<int64_t, double>>") == "[string: int | float]");
    }

    SECTION("Variant and optional types") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::variant<int64_t, std::string>") == "int | string");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::__1::variant<int64_t, double, bool>") == "int | float | bool");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::optional<int64_t>") == "int?");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::__1::optional<std::string>") == "string?");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::vector<std::variant<int64_t, std::string>>") == "[int | string]");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::optional<std::vector<int64_t>>") == "[int]?");
    }

    SECTION("Pointers and object references") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::shared_ptr<keet::rt::Object>") == "object");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("std::shared_ptr<MyClass>") == "MyClass");
    }

    SECTION("Internal compiler mangled prefixes") {
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("_keet_var_score") == "score");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("_keet_fn_calculate") == "calculate");
        REQUIRE(DiagnosticTranslator::demangle_cpp_type("keety_temp_buffer") == "temp_buffer");
    }
}

TEST_CASE("DiagnosticTranslator: Diagnostic Message Cleaning", "[diag][translator][clean]") {
    SECTION("Demangles quoted C++ types in error messages") {
        std::string raw = "no matching function for call to 'process(std::__1::vector<int64_t, std::allocator<int64_t>>&)'";
        std::string cleaned = DiagnosticTranslator::clean_diagnostic_message(raw);
        REQUIRE(cleaned == "no matching function or method for call to 'process([int])'");
    }

    SECTION("Cleans multiple types in conversion messages") {
        std::string raw = "cannot convert 'std::__1::basic_string<char>' to 'int64_t'";
        std::string cleaned = DiagnosticTranslator::clean_diagnostic_message(raw);
        REQUIRE(cleaned == "type mismatch: cannot convert 'string' to 'int'");
    }

    SECTION("Strips runtime namespace prefixes") {
        std::string raw = "call to 'keet::rt::io_println' does not match signature";
        std::string cleaned = DiagnosticTranslator::clean_diagnostic_message(raw);
        REQUIRE(cleaned.find("keet::rt::") == std::string::npos);
        REQUIRE(cleaned.find("io_println") != std::string::npos);
    }
}

TEST_CASE("DiagnosticTranslator: Header and Noise Detection", "[diag][translator][noise]") {
    SECTION("Identifies standard library internal headers") {
        REQUIRE(DiagnosticTranslator::is_internal_header("/usr/include/c++/v1/vector"));
        REQUIRE(DiagnosticTranslator::is_internal_header("/usr/include/c++/11/bits/stl_vector.h"));
        REQUIRE(DiagnosticTranslator::is_internal_header("C:\\Program Files\\MSVC\\include\\xutility"));
        REQUIRE(DiagnosticTranslator::is_internal_header("/path/to/keet/runtime.hpp"));
    }

    SECTION("Identifies user source files as non-internal") {
        REQUIRE_FALSE(DiagnosticTranslator::is_internal_header("src/main.keet"));
        REQUIRE_FALSE(DiagnosticTranslator::is_internal_header("/Users/dev/project/src/app.keet"));
        REQUIRE_FALSE(DiagnosticTranslator::is_internal_header("build/.keety_build/main.cpp"));
    }

    SECTION("Filters noise lines") {
        REQUIRE(DiagnosticTranslator::is_internal_noise_line("in instantiation of template class 'std::__1::vector<int>'"));
        REQUIRE(DiagnosticTranslator::is_internal_noise_line("expanded from macro 'KEETY_CHECK'"));
        REQUIRE(DiagnosticTranslator::is_internal_noise_line("note: candidate function template not viable: constraint not satisfied"));
    }
}

TEST_CASE("DiagnosticTranslator: Clang and GCC Output Parsing", "[diag][translator][clang_gcc]") {
    SECTION("Parses standard Clang error with column numbers") {
        std::string output =
            "src/main.keet:15:20: error: no member named 'hp' in 'Player'\n"
            "    return p.hp;\n"
            "           ~ ^~\n"
            "src/main.keet:15:20: note: did you mean 'health'?\n";

        auto diags = DiagnosticTranslator::parse_clang_gcc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].file == "src/main.keet");
        REQUIRE(diags[0].line == 15);
        REQUIRE(diags[0].column == 20);
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
        REQUIRE(diags[0].cleanMessage.find("no member named 'hp' in 'Player'") != std::string::npos);
        REQUIRE(diags[0].notes.size() == 1);
        REQUIRE(diags[0].notes[0].find("did you mean 'health'?") != std::string::npos);
    }

    SECTION("Suppresses internal STL template errors while keeping user errors") {
        std::string output =
            "/usr/include/c++/v1/vector:1200:10: error: static assertion failed\n"
            "src/main.keet:42:5: error: cannot convert 'std::string' to 'int64_t'\n";

        auto diags = DiagnosticTranslator::parse_clang_gcc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].file == "src/main.keet");
        REQUIRE(diags[0].line == 42);
        REQUIRE(diags[0].cleanMessage.find("type mismatch: cannot convert 'string' to 'int'") != std::string::npos);
    }

    SECTION("Parses GCC error with warning flags and notes") {
        std::string output =
            "src/utils.keet:24:9: error: 'class String' has no member named 'length_bytes' [-fpermissive]\n"
            "   24 |     str.length_bytes();\n"
            "      |         ^~~~~~~~~~~~\n"
            "src/utils.keet:24:9: note: in expansion of macro 'LEN'\n"
            "src/utils.keet:30:5: warning: unused variable 'unused_counter' [-Wunused-variable]\n";

        auto diags = DiagnosticTranslator::parse_clang_gcc_output(output);
        REQUIRE(diags.size() == 2);
        REQUIRE(diags[0].file == "src/utils.keet");
        REQUIRE(diags[0].line == 24);
        REQUIRE(diags[0].column == 9);
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
        REQUIRE(diags[0].cleanMessage.find("no member named 'length_bytes'") != std::string::npos);
        REQUIRE(diags[0].notes.size() == 1);
        REQUIRE(diags[0].notes[0].find("in expansion of macro 'LEN'") != std::string::npos);

        REQUIRE(diags[1].file == "src/utils.keet");
        REQUIRE(diags[1].line == 30);
        REQUIRE(diags[1].column == 5);
        REQUIRE(diags[1].severity == DiagnosticSeverity::Warning);
        REQUIRE(diags[1].cleanMessage.find("unused variable 'unused_counter'") != std::string::npos);
    }

    SECTION("Parses line-only GCC diagnostics without column numbers") {
        std::string output =
            "src/parser.keet:12: error: expected ';' before '}' token\n";

        auto diags = DiagnosticTranslator::parse_clang_gcc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].file == "src/parser.keet");
        REQUIRE(diags[0].line == 12);
        REQUIRE(diags[0].column == 0);
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
    }
}

TEST_CASE("DiagnosticTranslator: MSVC Output Parsing", "[diag][translator][msvc]") {
    SECTION("Parses MSVC error with code C2664") {
        std::string output =
            "src\\main.keet(25,12): error C2664: 'int add(int, int)': cannot convert argument 1 from 'std::string' to 'int'\n";

        auto diags = DiagnosticTranslator::parse_msvc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].file == "src\\main.keet");
        REQUIRE(diags[0].line == 25);
        REQUIRE(diags[0].column == 12);
        REQUIRE(diags[0].code == "C2664");
        REQUIRE(diags[0].cleanMessage.find("type mismatch: cannot convert argument 1 from 'string' to 'int'") != std::string::npos);
    }

    SECTION("Parses MSVC warning without column number and note") {
        std::string output =
            "src\\main.keet(18): warning C4101: 'x': unreferenced local variable\n"
            "src\\main.keet(20,5): note: see declaration of 'x'\n";

        auto diags = DiagnosticTranslator::parse_msvc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].file == "src\\main.keet");
        REQUIRE(diags[0].line == 18);
        REQUIRE(diags[0].column == 0);
        REQUIRE(diags[0].severity == DiagnosticSeverity::Warning);
        REQUIRE(diags[0].code == "C4101");
        REQUIRE(diags[0].notes.size() == 1);
        REQUIRE(diags[0].notes[0].find("see declaration of 'x'") != std::string::npos);
    }

    SECTION("Parses MSVC fatal error") {
        std::string output =
            "src\\app.keet(5,1): fatal error C1083: Cannot open include file: 'missing.h': No such file or directory\n";

        auto diags = DiagnosticTranslator::parse_msvc_output(output);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
        REQUIRE(diags[0].code == "C1083");
    }
}

TEST_CASE("DiagnosticTranslator: Translation to KeetScript Diagnostics", "[diag][translator][translate]") {
    SECTION("Translates Clang error to E3003 TypeMismatch") {
        std::string output =
            "src/app.keet:10:8: error: cannot convert 'std::string' to 'int64_t'\n";

        auto diags = DiagnosticTranslator::translate(output, CompilerKind::Clang);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E3003");
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
        REQUIRE(diags[0].location.line == 10);
        REQUIRE(diags[0].location.column == 8);
    }

    SECTION("Translates GCC errors across dialects") {
        std::string gcc_output =
            "src/app.keet:14:5: error: 'val' was not declared in this scope\n";

        auto diags = DiagnosticTranslator::translate(gcc_output, CompilerKind::GCC);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E2003");
        REQUIRE(diags[0].location.line == 14);
        REQUIRE(diags[0].location.column == 5);
    }

    SECTION("Translates MSVC errors across dialects") {
        std::string msvc_output =
            "src\\app.keet(22,10): error C2039: 'run': is not a member of 'Worker'\n";

        auto diags = DiagnosticTranslator::translate(msvc_output, CompilerKind::MSVC);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E3011");
        REQUIRE(diags[0].location.line == 22);
        REQUIRE(diags[0].location.column == 10);
    }

    SECTION("Translates unknown member error to E3011 InvalidMemberAccess") {
        std::string output =
            "src/app.keet:18:14: error: no member named 'speed' in 'Car'\n"
            "src/app.keet:18:14: note: did you mean 'maxSpeed'?\n";

        auto diags = DiagnosticTranslator::translate(output, CompilerKind::Clang);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E3011");
        REQUIRE(diags[0].footers.size() == 1);
        REQUIRE(diags[0].footers[0].kind == DiagnosticFooter::Kind::Help);
        REQUIRE(diags[0].footers[0].message.find("did you mean 'maxSpeed'?") != std::string::npos);
    }

    SECTION("Translates undeclared identifier to E2003 UndeclaredIdentifier") {
        std::string output =
            "src/app.keet:5:1: error: use of undeclared identifier 'foo'\n";

        auto diags = DiagnosticTranslator::translate(output, CompilerKind::Clang);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E2003");
    }

    SECTION("Unclassified errors fall back to E7001 BackendCompilationError") {
        std::string output =
            "src/app.keet:30:1: error: backend inline assembly constraint failure\n";

        auto diags = DiagnosticTranslator::translate(output, CompilerKind::Clang);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E7001");
    }
}

