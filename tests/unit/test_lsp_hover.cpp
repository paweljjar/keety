#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer getHover produces rich Markdown documentation", "[lsp][hover]") {
    Analyzer analyzer;

    SECTION("Keywords hover produces syntax templates and descriptions") {
        std::string code = "fn main(): void {\n    let x: int = 10;\n    zonk \"error\";\n}\n";
        Document doc("file:///keywords.keet", "keet", 1, code);

        // Hover over 'fn'
        auto hoverFn = analyzer.getHover(doc, Position{0, 0});
        REQUIRE(hoverFn.has_value());
        REQUIRE(hoverFn->contents.kind == "markdown");
        REQUIRE(hoverFn->contents.value.find("fn <name>(<params>)") != std::string::npos);
        REQUIRE(hoverFn->range.has_value());
        REQUIRE(hoverFn->range->start.character == 0);

        // Hover over 'let'
        auto hoverLet = analyzer.getHover(doc, Position{1, 4});
        REQUIRE(hoverLet.has_value());
        REQUIRE(hoverLet->contents.value.find("let <name>") != std::string::npos);

        // Hover over 'zonk'
        auto hoverZonk = analyzer.getHover(doc, Position{2, 4});
        REQUIRE(hoverZonk.has_value());
        REQUIRE(hoverZonk->contents.value.find("zonk <error>") != std::string::npos);
    }

    SECTION("Primitive types hover produces type documentation") {
        std::string code = "let count: int = 5;\nlet name: string = \"keet\";\n";
        Document doc("file:///types.keet", "keet", 1, code);

        // Hover over 'int' (line 0, col 11)
        auto hoverInt = analyzer.getHover(doc, Position{0, 11});
        REQUIRE(hoverInt.has_value());
        REQUIRE(hoverInt->contents.value.find("type int") != std::string::npos);
        REQUIRE(hoverInt->contents.value.find("64-bit signed integer") != std::string::npos);

        // Hover over 'string' (line 1, col 11)
        auto hoverStr = analyzer.getHover(doc, Position{1, 11});
        REQUIRE(hoverStr.has_value());
        REQUIRE(hoverStr->contents.value.find("type string") != std::string::npos);
    }

    SECTION("Standard library module and function hover") {
        std::string code =
            "import io;\n"
            "fn main(): void {\n"
            "    io.println(\"Hello\");\n"
            "}\n";
        Document doc("file:///stdlib.keet", "keet", 1, code);

        // Hover over 'io' on line 0, col 7
        auto hoverIo = analyzer.getHover(doc, Position{0, 7});
        REQUIRE(hoverIo.has_value());
        REQUIRE(hoverIo->contents.value.find("Standard Library Module: `io`") != std::string::npos);

        // Hover over 'println' on line 2, col 8
        auto hoverPrintln = analyzer.getHover(doc, Position{2, 8});
        REQUIRE(hoverPrintln.has_value());
        REQUIRE(hoverPrintln->contents.value.find("fn println") != std::string::npos);
        REQUIRE(hoverPrintln->contents.value.find("standard output") != std::string::npos);
    }

    SECTION("User functions with doc comments and signatures") {
        std::string code =
            "/// Computes the sum of two 64-bit integers.\n"
            "/// Returns the integer result.\n"
            "export fn add(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "fn main(): void {\n"
            "    let x = add(1, 2);\n"
            "}\n";
        Document doc("file:///func.keet", "keet", 1, code);

        // Hover on definition 'add' on line 2, col 11
        auto hoverDef = analyzer.getHover(doc, Position{2, 11});
        REQUIRE(hoverDef.has_value());
        REQUIRE(hoverDef->contents.value.find("export fn add(a: int, b: int): int") != std::string::npos);
        REQUIRE(hoverDef->contents.value.find("Computes the sum of two 64-bit integers.") != std::string::npos);
        REQUIRE(hoverDef->contents.value.find("Returns the integer result.") != std::string::npos);

        // Hover on invocation 'add' on line 6, col 13
        auto hoverCall = analyzer.getHover(doc, Position{6, 13});
        REQUIRE(hoverCall.has_value());
        REQUIRE(hoverCall->contents.value.find("export fn add(a: int, b: int): int") != std::string::npos);
        REQUIRE(hoverCall->contents.value.find("Computes the sum of two 64-bit integers.") != std::string::npos);

        // Hover on parameter 'a' on line 2, col 14
        auto hoverParam = analyzer.getHover(doc, Position{2, 14});
        REQUIRE(hoverParam.has_value());
        REQUIRE(hoverParam->contents.value.find("(parameter) a: int") != std::string::npos);
    }

    SECTION("Classes, fields, and methods hover") {
        std::string code =
            "/// Represents a 2D mathematical vector.\n"
            "class Vector {\n"
            "    let x: int;\n"
            "    let y: int;\n"
            "    /// Calculates the magnitude.\n"
            "    fn length(): int {\n"
            "        return this.x;\n"
            "    }\n"
            "}\n";
        Document doc("file:///class_hover.keet", "keet", 1, code);

        // Hover on 'Vector' on line 1, col 7
        auto hoverClass = analyzer.getHover(doc, Position{1, 7});
        REQUIRE(hoverClass.has_value());
        REQUIRE(hoverClass->contents.value.find("class Vector") != std::string::npos);
        REQUIRE(hoverClass->contents.value.find("2 field(s) and 1 method(s)") != std::string::npos);
        REQUIRE(hoverClass->contents.value.find("Represents a 2D mathematical vector.") != std::string::npos);

        // Hover on field 'x' on line 2, col 8
        auto hoverField = analyzer.getHover(doc, Position{2, 8});
        REQUIRE(hoverField.has_value());
        REQUIRE(hoverField->contents.value.find("(field) let x: int") != std::string::npos);

        // Hover on method 'length' on line 5, col 8
        auto hoverMethod = analyzer.getHover(doc, Position{5, 8});
        REQUIRE(hoverMethod.has_value());
        REQUIRE(hoverMethod->contents.value.find("(method) fn length(): int") != std::string::npos);
        REQUIRE(hoverMethod->contents.value.find("Calculates the magnitude.") != std::string::npos);
    }

    SECTION("Enums and enum members hover") {
        std::string code =
            "enum Color {\n"
            "    Red,\n"
            "    Green,\n"
            "    Blue\n"
            "}\n";
        Document doc("file:///enum_hover.keet", "keet", 1, code);

        // Hover on 'Color' on line 0, col 6
        auto hoverEnum = analyzer.getHover(doc, Position{0, 6});
        REQUIRE(hoverEnum.has_value());
        REQUIRE(hoverEnum->contents.value.find("enum Color") != std::string::npos);
        REQUIRE(hoverEnum->contents.value.find("3 variant(s)") != std::string::npos);

        // Hover on 'Green' on line 2, col 5
        auto hoverMember = analyzer.getHover(doc, Position{2, 5});
        REQUIRE(hoverMember.has_value());
        REQUIRE(hoverMember->contents.value.find("(enum member) Color.Green") != std::string::npos);
    }

    SECTION("Local variables and KML component hover") {
        std::string code =
            "import \"./App.kml\";\n"
            "fn main(): void {\n"
            "    let app: App = App();\n"
            "    println(app);\n"
            "}\n";
        Document doc("file:///main_test.keet", "keet", 1, code);

        // Hover on local variable 'app' (line 2, col 8)
        auto hoverApp = analyzer.getHover(doc, Position{2, 8});
        REQUIRE(hoverApp.has_value());
        CHECK(hoverApp->contents.value.find("let app: App") != std::string::npos);
        // Must NOT match stdlib http.app()
        CHECK(hoverApp->contents.value.find("Express-style") == std::string::npos);

        // Hover on usage 'app' in println(app) (line 3, col 12)
        auto hoverAppUsage = analyzer.getHover(doc, Position{3, 12});
        REQUIRE(hoverAppUsage.has_value());
        CHECK(hoverAppUsage->contents.value.find("let app: App") != std::string::npos);

        // Hover on imported KML component 'App' in type annotation (line 2, col 13)
        auto hoverComp = analyzer.getHover(doc, Position{2, 13});
        REQUIRE(hoverComp.has_value());
        CHECK(hoverComp->contents.value.find("component App") != std::string::npos);
    }

    SECTION("Whitespace or unknown position returns nullopt") {
        std::string code = "fn main(): void {}\n";
        Document doc("file:///empty_hover.keet", "keet", 1, code);

        auto hoverEmpty = analyzer.getHover(doc, Position{0, 18});
        REQUIRE_FALSE(hoverEmpty.has_value());
    }
}

TEST_CASE("Server handles textDocument/hover JSON-RPC request", "[lsp][server][hover]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    // 1. Initialize server and verify hoverProvider capability
    std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initRespOpt = server.handleRawMessage(initReq);
    REQUIRE(initRespOpt.has_value());

    keet::json::detail::Parser pInit(*initRespOpt);
    auto jInit = pInit.parse_document();
    REQUIRE(jInit.is_object());
    const auto& resMap = std::get<JsonValue::Object>(std::get<JsonValue::Object>(jInit.value).at("result").value);
    const auto& caps = std::get<JsonValue::Object>(resMap.at("capabilities").value);
    REQUIRE(std::get<bool>(caps.at("hoverProvider").value) == true);

    // 2. Open document with function
    std::string openMsg =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "  \"textDocument\": {"
        "    \"uri\": \"file:///server_hover.keet\","
        "    \"languageId\": \"keet\","
        "    \"version\": 1,"
        "    \"text\": \"/// Greets the caller\\nfn greet(): void {}\\n\""
        "  }"
        "}}";
    (void)server.handleRawMessage(openMsg);

    // 3. Request hover on 'greet' at line 1, character 4
    std::string hoverReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/hover\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///server_hover.keet\"},"
        "  \"position\": {\"line\": 1, \"character\": 4}"
        "}}";
    auto hoverRespOpt = server.handleRawMessage(hoverReq);
    REQUIRE(hoverRespOpt.has_value());

    keet::json::detail::Parser pHover(*hoverRespOpt);
    auto jHover = pHover.parse_document();
    REQUIRE(jHover.is_object());
    const auto& hoverRespObj = std::get<JsonValue::Object>(jHover.value);
    REQUIRE(std::get<int64_t>(hoverRespObj.at("id").value) == 2);

    const auto& hoverResult = std::get<JsonValue::Object>(hoverRespObj.at("result").value);
    const auto& contentsObj = std::get<JsonValue::Object>(hoverResult.at("contents").value);
    REQUIRE(std::get<std::string>(contentsObj.at("kind").value) == "markdown");
    std::string val = std::get<std::string>(contentsObj.at("value").value);
    REQUIRE(val.find("fn greet(): void") != std::string::npos);
    REQUIRE(val.find("Greets the caller") != std::string::npos);

    // 4. Request hover on whitespace returns null result
    std::string invalidReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///server_hover.keet\"},"
        "  \"position\": {\"line\": 5, \"character\": 0}"
        "}}";
    auto invalidRespOpt = server.handleRawMessage(invalidReq);
    REQUIRE(invalidRespOpt.has_value());

    keet::json::detail::Parser pInvalid(*invalidRespOpt);
    auto jInvalid = pInvalid.parse_document();
    const auto& invRespObj = std::get<JsonValue::Object>(jInvalid.value);
    REQUIRE(std::get<int64_t>(invRespObj.at("id").value) == 3);
    REQUIRE(invRespObj.at("result").is_null());
}
