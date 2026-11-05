#include <catch2/catch_test_macros.hpp>
#include <keet/test.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <memory>
#include <sstream>

using namespace keety::lsp;

namespace {

struct TestServerContext {
    std::istringstream in{""};
    std::ostringstream out{};
    Transport transport{in, out};
    Server server{std::move(transport)};

    TestServerContext() {
        (void)server.handleRawMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
    }
};

void openDocument(Server& server, const std::string& uri, const std::string& text) {
    std::ostringstream ss;
    ss << "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
       << "  \"textDocument\": {"
       << "    \"uri\": \"" << uri << "\","
       << "    \"languageId\": \"keet\","
       << "    \"version\": 1,"
       << "    \"text\": \"" << keet::test::jsonEscape(text) << "\""
       << "  }"
       << "}}";
    (void)server.handleRawMessage(ss.str());
}

} // namespace

TEST_CASE("LSP initialize advertises codeActionProvider", "[lsp][codeAction]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"initialize\",\"params\":{}}";
    auto respOpt = server.handleRawMessage(initReq);
    REQUIRE(respOpt.has_value());

    keet::json::detail::Parser p(*respOpt);
    auto jResp = p.parse_document();
    REQUIRE(jResp.is_object());
    const auto& respObj = std::get<JsonValue::Object>(jResp.value);
    REQUIRE(std::get<int64_t>(respObj.at("id").value) == 10);

    const auto& resMap = std::get<JsonValue::Object>(respObj.at("result").value);
    const auto& caps = std::get<JsonValue::Object>(resMap.at("capabilities").value);

    REQUIRE(caps.find("codeActionProvider") != caps.end());
    REQUIRE(std::get<bool>(caps.at("codeActionProvider").value) == true);
}

TEST_CASE("LSP analyzer merges live keetlint diagnostics", "[lsp][analyzer][lint]") {
    Analyzer analyzer;

    SECTION("Flags unused-import with keetlint source and accurate range") {
        std::string code =
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"test\");\n"
            "}\n";

        Document doc("file:///test.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);

        REQUIRE(diags.size() == 1);
        CHECK(diags[0].code == "L001");
        CHECK(diags[0].source == "keetlint");
        CHECK(diags[0].severity == DiagnosticSeverity::Warning);
        CHECK(diags[0].range.start.line == 1);
    }

    SECTION("Suppression comment stops lint diagnostic in LSP") {
        std::string code =
            "import io;\n"
            "// keetlint:disable-next-line unused-import\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"test\");\n"
            "}\n";

        Document doc("file:///suppressed.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);
        CHECK(diags.empty());
    }
}

TEST_CASE("LSP textDocument/codeAction returns Quick Fixes for lint violations", "[lsp][codeAction]") {
    TestServerContext ctx;

    SECTION("Returns Quick Fix for unused-import") {
        std::string code =
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"hello\");\n"
            "}\n";

        openDocument(ctx.server, "file:///app.keet", code);

        std::string actionReq =
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/codeAction\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///app.keet\"},"
            "  \"range\": {\"start\": {\"line\": 1, \"character\": 0}, \"end\": {\"line\": 1, \"character\": 12}},"
            "  \"context\": {\"diagnostics\": []}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(actionReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        REQUIRE(jResp.is_object());
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(std::get<int64_t>(respObj.at("id").value) == 2);

        const auto& actions = std::get<JsonValue::Array>(respObj.at("result").value);
        REQUIRE(actions.size() == 1);

        const auto& action = std::get<JsonValue::Object>(actions[0].value);
        CHECK(std::get<std::string>(action.at("title").value) == "Remove unused import");
        CHECK(std::get<std::string>(action.at("kind").value) == "quickfix");
        CHECK(std::get<bool>(action.at("isPreferred").value) == true);

        // Verify WorkspaceEdit
        REQUIRE(action.find("edit") != action.end());
        const auto& editObj = std::get<JsonValue::Object>(action.at("edit").value);
        const auto& changesObj = std::get<JsonValue::Object>(editObj.at("changes").value);
        REQUIRE(changesObj.find("file:///app.keet") != changesObj.end());

        const auto& textEdits = std::get<JsonValue::Array>(changesObj.at("file:///app.keet").value);
        REQUIRE(textEdits.size() == 1);
        const auto& textEdit = std::get<JsonValue::Object>(textEdits[0].value);
        CHECK(std::get<std::string>(textEdit.at("newText").value) == "");
    }

    SECTION("Returns empty actions when range does not intersect any diagnostic") {
        std::string code =
            "import io;\n"
            "import math;\n"
            "fn main(): void {\n"
            "    io.println(\"hello\");\n"
            "}\n";

        openDocument(ctx.server, "file:///app.keet", code);

        std::string actionReq =
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/codeAction\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///app.keet\"},"
            "  \"range\": {\"start\": {\"line\": 3, \"character\": 0}, \"end\": {\"line\": 3, \"character\": 10}},"
            "  \"context\": {\"diagnostics\": []}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(actionReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        const auto& actions = std::get<JsonValue::Array>(respObj.at("result").value);
        CHECK(actions.empty());
    }

    SECTION("Returns Quick Fix for unused-variable preserving type and initializer") {
        std::string code =
            "fn main(): void {\n"
            "    let secretKey: int = 9999;\n"
            "}\n";

        openDocument(ctx.server, "file:///var.keet", code);

        std::string actionReq =
            "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/codeAction\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///var.keet\"},"
            "  \"range\": {\"start\": {\"line\": 1, \"character\": 8}, \"end\": {\"line\": 1, \"character\": 17}},"
            "  \"context\": {\"diagnostics\": []}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(actionReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        const auto& actions = std::get<JsonValue::Array>(respObj.at("result").value);
        REQUIRE(actions.size() == 1);

        const auto& action = std::get<JsonValue::Object>(actions[0].value);
        CHECK(std::get<std::string>(action.at("title").value) == "Prefix unused variable with '_'");

        const auto& editObj = std::get<JsonValue::Object>(action.at("edit").value);
        const auto& changesObj = std::get<JsonValue::Object>(editObj.at("changes").value);
        const auto& textEdits = std::get<JsonValue::Array>(changesObj.at("file:///var.keet").value);
        REQUIRE(textEdits.size() == 1);
        const auto& textEdit = std::get<JsonValue::Object>(textEdits[0].value);
        CHECK(std::get<std::string>(textEdit.at("newText").value) == "_secretKey");

        // Verify edit range targets only 'secretKey' (col 8 to 17), NOT the whole line
        const auto& editRange = std::get<JsonValue::Object>(textEdit.at("range").value);
        const auto& startPos = std::get<JsonValue::Object>(editRange.at("start").value);
        const auto& endPos = std::get<JsonValue::Object>(editRange.at("end").value);
        CHECK(std::get<int64_t>(startPos.at("character").value) == 8);
        CHECK(std::get<int64_t>(endPos.at("character").value) == 17);
    }

    SECTION("Returns Quick Fix for unnecessary-mut removing only mut") {
        std::string code =
            "fn compute(mut prefix: string): string {\n"
            "    return prefix;\n"
            "}\n";

        openDocument(ctx.server, "file:///mut.keet", code);

        std::string actionReq =
            "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/codeAction\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///mut.keet\"},"
            "  \"range\": {\"start\": {\"line\": 0, \"character\": 11}, \"end\": {\"line\": 0, \"character\": 14}},"
            "  \"context\": {\"diagnostics\": []}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(actionReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        const auto& actions = std::get<JsonValue::Array>(respObj.at("result").value);
        REQUIRE(actions.size() == 1);

        const auto& action = std::get<JsonValue::Object>(actions[0].value);
        CHECK(std::get<std::string>(action.at("title").value) == "Remove unnecessary 'mut'");

        const auto& editObj = std::get<JsonValue::Object>(action.at("edit").value);
        const auto& changesObj = std::get<JsonValue::Object>(editObj.at("changes").value);
        const auto& textEdits = std::get<JsonValue::Array>(changesObj.at("file:///mut.keet").value);
        REQUIRE(textEdits.size() == 1);
        const auto& textEdit = std::get<JsonValue::Object>(textEdits[0].value);
        CHECK(std::get<std::string>(textEdit.at("newText").value) == "");

        // Verify edit range targets "mut " (col 11 to 15), preserving parameter name and type
        const auto& editRange = std::get<JsonValue::Object>(textEdit.at("range").value);
        const auto& startPos = std::get<JsonValue::Object>(editRange.at("start").value);
        const auto& endPos = std::get<JsonValue::Object>(editRange.at("end").value);
        CHECK(std::get<int64_t>(startPos.at("character").value) == 11);
        CHECK(std::get<int64_t>(endPos.at("character").value) == 15);
    }
}
