#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/diagnostic_translator.hpp>
#include <keety/diag/source_manager.hpp>

using namespace keety::diag;

TEST_CASE("DiagnosticTranslator: Symbol Demangling and Package Suggestion", "[diag][linker][symbols]") {
    SECTION("Demangles C and C++ symbol names") {
        REQUIRE(DiagnosticTranslator::demangle_symbol("_sqlite3_open") == "sqlite3_open");
        REQUIRE(DiagnosticTranslator::demangle_symbol("_SSL_connect") == "SSL_connect");
        REQUIRE(DiagnosticTranslator::demangle_symbol("_keet_fn_calculate") == "calculate");
        REQUIRE(DiagnosticTranslator::demangle_symbol("_keet_var_score") == "score");
        REQUIRE(DiagnosticTranslator::demangle_symbol("keety_runtime_init") == "runtime_init");

#if defined(KEETY_HAS_CXXABI)
        // Itanium C++ demangling: _Z3fooi -> foo(int)
        REQUIRE(DiagnosticTranslator::demangle_symbol("_Z3fooi") == "foo(int)");
#endif
    }

    SECTION("Suggests appropriate native package dependencies") {
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("sqlite3_open") == "sqlite");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("sqlite3_close") == "sqlite");

        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("SSL_connect") == "openssl");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("EVP_sha256") == "openssl");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("CRYPTO_free") == "openssl");

        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("curl_easy_init") == "libcurl");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("deflate") == "zlib");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("pthread_create") == "pthreads (-pthread)");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("sin") == "math library (-lm)");
        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("sqrt") == "math library (-lm)");

        REQUIRE(DiagnosticTranslator::suggest_package_for_symbol("custom_unknown_function").empty());
    }
}

TEST_CASE("DiagnosticTranslator: macOS ld64 Output Parsing", "[diag][linker][ld64]") {
    std::string ld64_output =
        "Undefined symbols for architecture arm64:\n"
        "  \"_sqlite3_open\", referenced from:\n"
        "      _keet_main in main.o\n"
        "      _init_db in db.o\n"
        "  \"_SSL_connect\", referenced from:\n"
        "      _connect_socket in net.o\n"
        "ld: symbol(s) not found for architecture arm64\n"
        "clang: error: linker command failed with exit code 1 (use -v to see invocation)\n";

    auto symbols = DiagnosticTranslator::parse_linker_output(ld64_output);
    REQUIRE(symbols.size() == 2);

    REQUIRE(symbols[0].rawSymbol == "_sqlite3_open");
    REQUIRE(symbols[0].demangledSymbol == "sqlite3_open");
    REQUIRE(symbols[0].suggestedPackage == "sqlite");
    REQUIRE(symbols[0].referencedFrom.find("_keet_main in main.o") != std::string::npos);

    REQUIRE(symbols[1].rawSymbol == "_SSL_connect");
    REQUIRE(symbols[1].demangledSymbol == "SSL_connect");
    REQUIRE(symbols[1].suggestedPackage == "openssl");
    REQUIRE(symbols[1].referencedFrom.find("_connect_socket in net.o") != std::string::npos);

    auto diags = DiagnosticTranslator::translate_linker_output(ld64_output);
    REQUIRE(diags.size() == 2);

    REQUIRE(diags[0].code == "E6001");
    REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
    REQUIRE(diags[0].message == "unresolved external symbol 'sqlite3_open'");
    REQUIRE(diags[0].footers.size() >= 2);
    REQUIRE(diags[0].footers[0].message.find("_keet_main in main.o") != std::string::npos);
    REQUIRE(diags[0].footers[1].message.find("declare package dependency 'sqlite' in keet.toml") != std::string::npos);

    REQUIRE(diags[1].code == "E6001");
    REQUIRE(diags[1].message == "unresolved external symbol 'SSL_connect'");
    REQUIRE(diags[1].footers[1].message.find("declare package dependency 'openssl' in keet.toml") != std::string::npos);
}

TEST_CASE("DiagnosticTranslator: GNU ld Output Parsing", "[diag][linker][gnu_ld]") {
    std::string gnu_output =
        "/usr/bin/ld: main.o: in function `_keet_main':\n"
        "main.cpp:(.text+0x1a): undefined reference to `sqlite3_open'\n"
        "/usr/bin/ld: main.o: undefined reference to 'curl_easy_init'\n"
        "collect2: error: ld returned 1 exit status\n";

    auto symbols = DiagnosticTranslator::parse_linker_output(gnu_output);
    REQUIRE(symbols.size() == 2);

    REQUIRE(symbols[0].demangledSymbol == "sqlite3_open");
    REQUIRE(symbols[0].suggestedPackage == "sqlite");

    REQUIRE(symbols[1].demangledSymbol == "curl_easy_init");
    REQUIRE(symbols[1].suggestedPackage == "libcurl");

    auto diags = DiagnosticTranslator::translate_linker_output(gnu_output);
    REQUIRE(diags.size() == 2);

    REQUIRE(diags[0].code == "E6001");
    REQUIRE(diags[0].message == "unresolved external symbol 'sqlite3_open'");
    REQUIRE(diags[0].footers.size() >= 1);
    REQUIRE(diags[0].footers.back().message.find("declare package dependency 'sqlite' in keet.toml") != std::string::npos);

    REQUIRE(diags[1].code == "E6001");
    REQUIRE(diags[1].message == "unresolved external symbol 'curl_easy_init'");
    REQUIRE(diags[1].footers.back().message.find("declare package dependency 'libcurl' in keet.toml") != std::string::npos);
}

TEST_CASE("DiagnosticTranslator: MSVC link.exe Output Parsing", "[diag][linker][msvc]") {
    std::string msvc_output =
        "main.obj : error LNK2019: unresolved external symbol sqlite3_open referenced in function _keet_main\n"
        "main.obj : error LNK2001: unresolved external symbol SSL_connect\n"
        "fatal error LNK1120: 2 unresolved externals\n";

    auto symbols = DiagnosticTranslator::parse_linker_output(msvc_output);
    REQUIRE(symbols.size() == 2);

    REQUIRE(symbols[0].demangledSymbol == "sqlite3_open");
    REQUIRE(symbols[0].suggestedPackage == "sqlite");
    REQUIRE(symbols[0].referencedFrom == "_keet_main");

    REQUIRE(symbols[1].demangledSymbol == "SSL_connect");
    REQUIRE(symbols[1].suggestedPackage == "openssl");

    auto diags = DiagnosticTranslator::translate_linker_output(msvc_output);
    REQUIRE(diags.size() == 2);

    REQUIRE(diags[0].code == "E6001");
    REQUIRE(diags[0].message == "unresolved external symbol 'sqlite3_open'");
    REQUIRE(diags[0].footers.size() == 2);
    REQUIRE(diags[0].footers[0].message == "referenced from _keet_main");
    REQUIRE(diags[0].footers[1].message.find("declare package dependency 'sqlite' in keet.toml") != std::string::npos);
}

TEST_CASE("DiagnosticTranslator: Missing Native Library Diagnostics", "[diag][linker][libraries]") {
    SECTION("macOS missing library flag") {
        std::string out = "ld: library not found for -lsqlite3\nclang: error: linker command failed with exit code 1\n";
        auto diags = DiagnosticTranslator::translate_linker_output(out);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E6001");
        REQUIRE(diags[0].message == "native library 'sqlite3' was not found by the linker");
        REQUIRE(diags[0].footers.size() == 1);
        REQUIRE(diags[0].footers[0].message.find("install library 'sqlite3'") != std::string::npos);
    }

    SECTION("GNU ld cannot find library") {
        std::string out = "/usr/bin/ld: cannot find -lssl\ncollect2: error: ld returned 1 exit status\n";
        auto diags = DiagnosticTranslator::translate_linker_output(out);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E6001");
        REQUIRE(diags[0].message == "native library 'ssl' was not found by the linker");
    }

    SECTION("MSVC cannot open library file") {
        std::string out = "LINK : fatal error LNK1104: cannot open file 'curl.lib'\n";
        auto diags = DiagnosticTranslator::translate_linker_output(out);
        REQUIRE(diags.size() == 1);
        REQUIRE(diags[0].code == "E6001");
        REQUIRE(diags[0].message == "native library 'curl' was not found by the linker");
    }
}

TEST_CASE("DiagnosticTranslator: Linker Diagnostic Formatting in Emitter", "[diag][linker][emitter]") {
    SourceManager sm;
    std::string ld_out =
        "Undefined symbols for architecture arm64:\n"
        "  \"_sqlite3_open\", referenced from:\n"
        "      _keet_main in main.o\n";

    auto diags = DiagnosticTranslator::translate_linker_output(ld_out);
    REQUIRE(diags.size() == 1);

    DiagnosticEmitter emitter(sm, ColorMode::Never);
    std::string formatted = emitter.format(diags[0]);

    REQUIRE(formatted.find("error[E6001]: unresolved external symbol 'sqlite3_open'") != std::string::npos);
    REQUIRE(formatted.find("= note: referenced from _keet_main in main.o") != std::string::npos);
    REQUIRE(formatted.find("= help: did you forget to declare package dependency 'sqlite' in keet.toml?") != std::string::npos);
}

