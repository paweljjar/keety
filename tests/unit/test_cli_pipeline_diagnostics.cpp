#include <catch2/catch_test_macros.hpp>
#include <keet/json.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/diagnostic_translator.hpp>
#include <keety/driver/cli_driver.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety;
using namespace keety::diag;
using namespace keety::driver;

TEST_CASE("CliDriver Pipeline: Diagnostic Formatting and ICE Confinement", "[driver][pipeline][diagnostics]") {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "keety_pipeline_test";
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);

    std::filesystem::path src_file = temp_dir / "app.keet";
    {
        std::ofstream f(src_file);
        f << "fn main() {\n"
          << "    let x = 42;\n"
          << "    io.println(x);\n"
          << "}\n";
    }

    SECTION("DiagnosticEmitter formats clean terminal output without ICE prompts") {
        SourceManager sm;
        sm.addBuffer("src/main.keet", "fn test() {\n    let val = \"hello\";\n    let num = val + 1;\n}\n");

        std::string raw_compiler_err =
            "src/main.keet:3:15: error: cannot convert 'std::string' to 'int64_t'\n";

        auto diags = DiagnosticTranslator::translate(raw_compiler_err, CompilerKind::Clang, &sm, nullptr);
        REQUIRE(diags.size() == 1);

        DiagnosticEmitter emitter(sm, ColorMode::Never);
        std::string formatted = emitter.format(diags[0]);

        // Clean KeetScript error
        REQUIRE(formatted.find("error[E3003]") != std::string::npos);
        REQUIRE(formatted.find("--> src/main.keet:3:15") != std::string::npos);
        REQUIRE(formatted.find("let num = val + 1;") != std::string::npos);
        REQUIRE(formatted.find("^^^") != std::string::npos);

        // Strict ICE confinement: NO generic crash or ICE prompts
        REQUIRE(formatted.find("error[ICE]") == std::string::npos);
        REQUIRE(formatted.find("Please file a bug report") == std::string::npos);
        REQUIRE(formatted.find("Run with '--verbose-cpp'") == std::string::npos);
    }

    SECTION("DiagnosticEmitter outputs valid JSON array for backend diagnostics") {
        SourceManager sm;
        sm.addBuffer("src/app.keet", "fn calculate() {\n    return 42;\n}\n");

        std::string raw_compiler_err =
            "src/app.keet:2:5: error: use of undeclared identifier 'unknown_var'\n";

        auto diags = DiagnosticTranslator::translate(raw_compiler_err, CompilerKind::Clang, &sm, nullptr);
        REQUIRE(diags.size() == 1);

        DiagnosticEmitter emitter(sm, ColorMode::Never);
        emitter.report(diags[0]);

        std::string json_str = emitter.formatJson();
        REQUIRE_FALSE(json_str.empty());

        // Parse with JSON parser
        ::keet::json::detail::Parser parser(json_str);
        auto doc = parser.parse_document();
        REQUIRE(std::holds_alternative<::keet::json::detail::JsonNode::Array>(doc.value));

        const auto& arr = std::get<::keet::json::detail::JsonNode::Array>(doc.value);
        REQUIRE(arr.size() == 1);

        const auto& obj = std::get<::keet::json::detail::JsonNode::Object>(arr[0].value);
        REQUIRE(obj.find("code") != obj.end());
        REQUIRE(std::get<std::string>(obj.at("code").value) == "E2003");
        REQUIRE(std::get<std::string>(obj.at("severity").value) == "error");
    }

    SECTION("Linker translation formats structured E6001 with remediation advice") {
        std::string linker_err =
            "Undefined symbols for architecture arm64:\n"
            "  \"_sqlite3_open\", referenced from:\n"
            "      _keet_main in main.o\n";

        auto diags = DiagnosticTranslator::translate_linker_output(linker_err);
        REQUIRE(diags.size() == 1);

        SourceManager sm;
        DiagnosticEmitter emitter(sm, ColorMode::Never);
        std::string formatted = emitter.format(diags[0]);

        REQUIRE(formatted.find("error[E6001]: unresolved external symbol 'sqlite3_open'") != std::string::npos);
        REQUIRE(formatted.find("= note: referenced from _keet_main in main.o") != std::string::npos);
        REQUIRE(formatted.find("= help: did you forget to declare package dependency 'sqlite' in keet.toml?") != std::string::npos);

        // Strict ICE confinement
        REQUIRE(formatted.find("error[ICE]") == std::string::npos);
        REQUIRE(formatted.find("Please file a bug report") == std::string::npos);
    }

    SECTION("Linker JSON output conformance") {
        std::string linker_err =
            "Undefined symbols for architecture arm64:\n"
            "  \"_SSL_connect\", referenced from:\n"
            "      _net_connect in net.o\n";

        auto diags = DiagnosticTranslator::translate_linker_output(linker_err);
        REQUIRE(diags.size() == 1);

        SourceManager sm;
        DiagnosticEmitter emitter(sm, ColorMode::Never);
        emitter.report(diags[0]);

        std::string json_str = emitter.formatJson();
        REQUIRE_FALSE(json_str.empty());

        ::keet::json::detail::Parser parser(json_str);
        auto doc = parser.parse_document();
        REQUIRE(std::holds_alternative<::keet::json::detail::JsonNode::Array>(doc.value));

        const auto& arr = std::get<::keet::json::detail::JsonNode::Array>(doc.value);
        REQUIRE(arr.size() == 1);

        const auto& obj = std::get<::keet::json::detail::JsonNode::Object>(arr[0].value);
        REQUIRE(std::get<std::string>(obj.at("code").value) == "E6001");
        REQUIRE(std::get<std::string>(obj.at("message").value).find("SSL_connect") != std::string::npos);
    }

    std::filesystem::remove_all(temp_dir, ec);
}
