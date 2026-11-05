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

TEST_CASE("LSP initialize advertises documentFormattingProvider", "[lsp][formatting]") {
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

    REQUIRE(caps.find("documentFormattingProvider") != caps.end());
    REQUIRE(std::get<bool>(caps.at("documentFormattingProvider").value) == true);
}

TEST_CASE("LSP textDocument/formatting returns canonical edits", "[lsp][formatting]") {
    TestServerContext ctx;

    SECTION("Formatting unformatted code returns replacement TextEdit") {
        std::string unformatted =
            "fn   computeSum  ( a :int ,b:int ) :int {\n"
            "let   total :int =a+b ; \n"
            "return   total;\n"
            "}\n";

        openDocument(ctx.server, "file:///sum.keet", unformatted);

        std::string formatReq =
            "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/formatting\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///sum.keet\"},"
            "  \"options\": {\"tabSize\": 4, \"insertSpaces\": true}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(formatReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        REQUIRE(jResp.is_object());
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(std::get<int64_t>(respObj.at("id").value) == 2);

        REQUIRE(respObj.at("result").is_array());
        const auto& edits = std::get<JsonValue::Array>(respObj.at("result").value);
        REQUIRE(edits.size() == 1);

        const auto& editObj = std::get<JsonValue::Object>(edits[0].value);
        const auto& rangeObj = std::get<JsonValue::Object>(editObj.at("range").value);
        const auto& startObj = std::get<JsonValue::Object>(rangeObj.at("start").value);
        REQUIRE(std::get<int64_t>(startObj.at("line").value) == 0);
        REQUIRE(std::get<int64_t>(startObj.at("character").value) == 0);

        std::string newText = std::get<std::string>(editObj.at("newText").value);
        std::string expected =
            "fn computeSum(a: int, b: int): int {\n"
            "    let total: int = a + b;\n"
            "    return total;\n"
            "}\n";
        CHECK(newText == expected);
    }

    SECTION("Formatting already canonical code returns empty array") {
        std::string clean =
            "fn computeSum(a: int, b: int): int {\n"
            "    let total: int = a + b;\n"
            "    return total;\n"
            "}\n";

        openDocument(ctx.server, "file:///clean.keet", clean);

        std::string formatReq =
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/formatting\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///clean.keet\"},"
            "  \"options\": {\"tabSize\": 4, \"insertSpaces\": true}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(formatReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(respObj.at("result").is_array());
        const auto& edits = std::get<JsonValue::Array>(respObj.at("result").value);
        CHECK(edits.empty());
    }

    SECTION("Formatting unknown document returns empty array") {
        std::string formatReq =
            "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/formatting\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///missing.keet\"},"
            "  \"options\": {\"tabSize\": 4, \"insertSpaces\": true}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(formatReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(respObj.at("result").is_array());
        const auto& edits = std::get<JsonValue::Array>(respObj.at("result").value);
        CHECK(edits.empty());
    }

    SECTION("Formatting broken syntax returns empty array without altering document") {
        std::string broken = "fn invalid syntax {{{ }}}";
        openDocument(ctx.server, "file:///broken.keet", broken);

        std::string formatReq =
            "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/formatting\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///broken.keet\"},"
            "  \"options\": {\"tabSize\": 4, \"insertSpaces\": true}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(formatReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(respObj.at("result").is_array());
        const auto& edits = std::get<JsonValue::Array>(respObj.at("result").value);
        CHECK(edits.empty());
    }

    SECTION("Formatting with custom tabSize 2") {
        std::string code =
            "fn test(): void {\n"
            "let x: int = 1;\n"
            "}\n";

        openDocument(ctx.server, "file:///indent2.keet", code);

        std::string formatReq =
            "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"textDocument/formatting\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///indent2.keet\"},"
            "  \"options\": {\"tabSize\": 2, \"insertSpaces\": true}"
            "}}";

        auto respOpt = ctx.server.handleRawMessage(formatReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(respObj.at("result").is_array());
        const auto& edits = std::get<JsonValue::Array>(respObj.at("result").value);
        REQUIRE(edits.size() == 1);

        const auto& editObj = std::get<JsonValue::Object>(edits[0].value);
        std::string newText = std::get<std::string>(editObj.at("newText").value);
        CHECK(newText == "fn test(): void {\n  let x: int = 1;\n}\n");
    }
}
