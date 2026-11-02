#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer findDefinition resolves symbols and declarations", "[lsp][definition]") {
    Analyzer analyzer;

    SECTION("Resolves local function declaration") {
        std::string code =
            "fn compute(x: int): int {\n"   // line 0
            "    return x + 1;\n"           // line 1
            "}\n"                           // line 2
            "fn main(): void {\n"          // line 3
            "    let res = compute(10);\n" // line 4: 'compute' is at line 4, col 14
            "}\n";

        Document doc("file:///main.keet", "keet", 1, code);
        // Cursor on 'compute' in line 4, col 16
        auto loc = analyzer.findDefinition(doc, Position{4, 16});
        REQUIRE(loc.has_value());
        REQUIRE(loc->uri == "file:///main.keet");
        REQUIRE(loc->range.start.line == 0);
        REQUIRE(loc->range.start.character == 0);
    }

    SECTION("Resolves function parameter inside enclosing function") {
        std::string code =
            "fn doubleVal(val: int): int {\n" // line 0: 'val' at line 0, col 13
            "    return val * 2;\n"           // line 1: 'val' at line 1, col 11
            "}\n";

        Document doc("file:///param.keet", "keet", 1, code);
        // Cursor on 'val' in line 1, col 12
        auto loc = analyzer.findDefinition(doc, Position{1, 12});
        REQUIRE(loc.has_value());
        REQUIRE(loc->uri == "file:///param.keet");
        REQUIRE(loc->range.start.line == 0);
        REQUIRE(loc->range.start.character == 13);
    }

    SECTION("Resolves top-level variable and constant") {
        std::string code =
            "let globalCounter = 100;\n"      // line 0
            "const MAX_LIMIT = 500;\n"        // line 1
            "fn check(): int {\n"             // line 2
            "    return globalCounter;\n"     // line 3: col 11
            "}\n";

        Document doc("file:///vars.keet", "keet", 1, code);
        // Cursor on 'globalCounter' in line 3
        auto loc = analyzer.findDefinition(doc, Position{3, 14});
        REQUIRE(loc.has_value());
        REQUIRE(loc->range.start.line == 0);

        // Check self-definition on MAX_LIMIT in line 1
        auto locConst = analyzer.findDefinition(doc, Position{1, 8});
        REQUIRE(locConst.has_value());
        REQUIRE(locConst->range.start.line == 1);
    }

    SECTION("Resolves class, field, and method declarations") {
        std::string code =
            "class Vector {\n"                // line 0
            "    let x: int;\n"               // line 1
            "    fn magnitude(): int {\n"     // line 2
            "        return this.x;\n"        // line 3
            "    }\n"                         // line 4
            "}\n"                             // line 5
            "fn test(): void {\n"             // line 6
            "    let v = Vector();\n"         // line 7: 'Vector' at line 7, col 13
            "}\n";

        Document doc("file:///class.keet", "keet", 1, code);
        // Cursor on 'Vector' instantiation in line 7
        auto locClass = analyzer.findDefinition(doc, Position{7, 14});
        REQUIRE(locClass.has_value());
        REQUIRE(locClass->range.start.line == 0);

        // Cursor on method 'magnitude' in line 2
        auto locMethod = analyzer.findDefinition(doc, Position{2, 9});
        REQUIRE(locMethod.has_value());
        REQUIRE(locMethod->range.start.line == 2);

        // Cursor on field 'x' in line 1
        auto locField = analyzer.findDefinition(doc, Position{1, 9});
        REQUIRE(locField.has_value());
        REQUIRE(locField->range.start.line == 1);
    }

    SECTION("Resolves enum and enum member declarations") {
        std::string code =
            "enum Status {\n"                 // line 0
            "    Pending,\n"                  // line 1
            "    Completed\n"                 // line 2
            "}\n";

        Document doc("file:///enum.keet", "keet", 1, code);
        // Cursor on 'Status'
        auto locEnum = analyzer.findDefinition(doc, Position{0, 6});
        REQUIRE(locEnum.has_value());
        REQUIRE(locEnum->range.start.line == 0);

        // Cursor on 'Completed'
        auto locMember = analyzer.findDefinition(doc, Position{2, 6});
        REQUIRE(locMember.has_value());
        REQUIRE(locMember->range.start.line == 2);
    }

    SECTION("Resolves relative import string literal to file URI") {
        // Create temporary directory with main.keet and helper.keet
        std::error_code ec;
        auto tempDir = std::filesystem::temp_directory_path() / "keet_lsp_test_import";
        std::filesystem::create_directories(tempDir, ec);

        std::filesystem::path helperFile = tempDir / "helper.keet";
        std::ofstream helperOut(helperFile);
        helperOut << "pub fn help(): void {}\n";
        helperOut.close();

        std::filesystem::path mainFile = tempDir / "main.keet";
        std::string mainCode =
            "import \"./helper\";\n"
            "fn main(): void {}\n";
        std::ofstream mainOut(mainFile);
        mainOut << mainCode;
        mainOut.close();

        std::string mainUri = "file://" + std::filesystem::canonical(mainFile).generic_string();
        std::string helperUri = "file://" + std::filesystem::canonical(helperFile).generic_string();

        Document doc(mainUri, "keet", 1, mainCode);
        // Cursor on "./helper" in line 0, col 10
        auto loc = analyzer.findDefinition(doc, Position{0, 10});
        REQUIRE(loc.has_value());
        REQUIRE(loc->uri == helperUri);
        REQUIRE(loc->range.start.line == 0);
        REQUIRE(loc->range.start.character == 0);

        std::filesystem::remove_all(tempDir, ec);
    }

    SECTION("Returns nullopt for unknown symbol or whitespace") {
        std::string code = "fn main(): void {}\n";
        Document doc("file:///empty.keet", "keet", 1, code);
        // Empty whitespace at end of line 0
        auto loc = analyzer.findDefinition(doc, Position{0, 18});
        REQUIRE_FALSE(loc.has_value());
    }
}

TEST_CASE("Server handles textDocument/definition JSON-RPC request", "[lsp][server][definition]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    // 1. Initialize server and verify definitionProvider capability
    std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initRespOpt = server.handleRawMessage(initReq);
    REQUIRE(initRespOpt.has_value());

    keet::json::detail::Parser pInit(*initRespOpt);
    auto jInit = pInit.parse_document();
    REQUIRE(jInit.is_object());
    const auto& resMap = std::get<JsonValue::Object>(std::get<JsonValue::Object>(jInit.value).at("result").value);
    const auto& caps = std::get<JsonValue::Object>(resMap.at("capabilities").value);
    REQUIRE(std::get<bool>(caps.at("definitionProvider").value) == true);

    // 2. Open document with a function and a call to it
    std::string code =
        "fn greet(): void {}\n"
        "fn main(): void {\n"
        "    greet();\n"
        "}\n";

    std::string openMsg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "  \"textDocument\": {"
        "    \"uri\": \"file:///greet.keet\","
        "    \"languageId\": \"keet\","
        "    \"version\": 1,"
        "    \"text\": \"fn greet(): void {}\\nfn main(): void {\\n    greet();\\n}\\n\""
        "  }"
        "}}";
    (void)server.handleRawMessage(openMsg);

    // 3. Request definition of 'greet' at line 2, character 6
    std::string defReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///greet.keet\"},"
        "  \"position\": {\"line\": 2, \"character\": 6}"
        "}}";
    auto defRespOpt = server.handleRawMessage(defReq);
    REQUIRE(defRespOpt.has_value());

    keet::json::detail::Parser pDef(*defRespOpt);
    auto jDef = pDef.parse_document();
    REQUIRE(jDef.is_object());
    const auto& defRespObj = std::get<JsonValue::Object>(jDef.value);
    REQUIRE(std::get<int64_t>(defRespObj.at("id").value) == 2);

    const auto& defResult = std::get<JsonValue::Object>(defRespObj.at("result").value);
    REQUIRE(std::get<std::string>(defResult.at("uri").value) == "file:///greet.keet");

    const auto& rangeObj = std::get<JsonValue::Object>(defResult.at("range").value);
    const auto& startObj = std::get<JsonValue::Object>(rangeObj.at("start").value);
    REQUIRE(std::get<int64_t>(startObj.at("line").value) == 0);
    REQUIRE(std::get<int64_t>(startObj.at("character").value) == 0);

    // 4. Request definition for non-existent position returns null result
    std::string invalidReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/definition\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///greet.keet\"},"
        "  \"position\": {\"line\": 10, \"character\": 0}"
        "}}";
    auto invalidRespOpt = server.handleRawMessage(invalidReq);
    REQUIRE(invalidRespOpt.has_value());

    keet::json::detail::Parser pInvalid(*invalidRespOpt);
    auto jInvalid = pInvalid.parse_document();
    const auto& invRespObj = std::get<JsonValue::Object>(jInvalid.value);
    REQUIRE(std::get<int64_t>(invRespObj.at("id").value) == 3);
    REQUIRE(invRespObj.at("result").is_null());
}
