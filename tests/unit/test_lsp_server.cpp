#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Document line offsets and position translation", "[lsp][document]") {
    std::string text =
        "fn add(a: int, b: int): int {\n"   // line 0: 0..29, length 30
        "    return a + b;\n"               // line 1: 30..47, length 18
        "}\n";                              // line 2: 48..49, length 2

    Document doc("file:///main.keet", "keet", 1, text);
    REQUIRE(doc.lineCount() == 4);

    SECTION("positionToOffset") {
        // Line 0, character 0
        auto off0 = doc.positionToOffset(Position{0, 0});
        REQUIRE(off0.has_value());
        REQUIRE(*off0 == 0);

        // Line 0, character 3 ('a' in add)
        auto off1 = doc.positionToOffset(Position{0, 3});
        REQUIRE(off1.has_value());
        REQUIRE(*off1 == 3);

        // Line 1, character 4 ('r' in return)
        auto off2 = doc.positionToOffset(Position{1, 4});
        REQUIRE(off2.has_value());
        REQUIRE(*off2 == 34);

        // Out of bounds line
        auto offInvalid = doc.positionToOffset(Position{99, 0});
        REQUIRE_FALSE(offInvalid.has_value());
    }

    SECTION("offsetToPosition") {
        Position p0 = doc.offsetToPosition(0);
        REQUIRE(p0.line == 0);
        REQUIRE(p0.character == 0);

        Position p1 = doc.offsetToPosition(34);
        REQUIRE(p1.line == 1);
        REQUIRE(p1.character == 4);

        Position p2 = doc.offsetToPosition(48);
        REQUIRE(p2.line == 2);
        REQUIRE(p2.character == 0);
    }
}

TEST_CASE("Server lifecycle and document sync", "[lsp][server]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    SECTION("initialize request returns server capabilities") {
        std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
        auto respOpt = server.handleRawMessage(initReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        REQUIRE(jResp.is_object());
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(std::get<int64_t>(respObj.at("id").value) == 1);

        const auto& resMap = std::get<JsonValue::Object>(respObj.at("result").value);
        const auto& caps = std::get<JsonValue::Object>(resMap.at("capabilities").value);
        const auto& sync = std::get<JsonValue::Object>(caps.at("textDocumentSync").value);
        REQUIRE(std::get<bool>(sync.at("openClose").value) == true);
        REQUIRE(std::get<int64_t>(sync.at("change").value) == 1);

        const auto& serverInfo = std::get<JsonValue::Object>(resMap.at("serverInfo").value);
        REQUIRE(std::get<std::string>(serverInfo.at("name").value) == "keet-lsp");
        REQUIRE(server.isInitialized());
    }

    SECTION("textDocument didOpen, didChange, and didClose") {
        // Initialize server first
        (void)server.handleRawMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");

        // didOpen notification
        std::string openNotice =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
            "  \"textDocument\": {"
            "    \"uri\": \"file:///app.keet\","
            "    \"languageId\": \"keet\","
            "    \"version\": 1,"
            "    \"text\": \"let x = 10;\""
            "  }"
            "}}";
        (void)server.handleRawMessage(openNotice);

        REQUIRE(server.documentManager().hasDocument("file:///app.keet"));
        auto docOpt = server.documentManager().getDocument("file:///app.keet");
        REQUIRE(docOpt.has_value());
        REQUIRE(docOpt->text() == "let x = 10;");
        REQUIRE(docOpt->version() == 1);

        // didChange notification
        std::string changeNotice =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///app.keet\", \"version\": 2},"
            "  \"contentChanges\": [{\"text\": \"let x = 20; let y = 30;\"}]"
            "}}";
        (void)server.handleRawMessage(changeNotice);

        auto updatedDoc = server.documentManager().getDocument("file:///app.keet");
        REQUIRE(updatedDoc.has_value());
        REQUIRE(updatedDoc->text() == "let x = 20; let y = 30;");
        REQUIRE(updatedDoc->version() == 2);

        // didClose notification
        std::string closeNotice =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///app.keet\"}"
            "}}";
        (void)server.handleRawMessage(closeNotice);
        REQUIRE_FALSE(server.documentManager().hasDocument("file:///app.keet"));
    }

    SECTION("shutdown and exit lifecycle") {
        (void)server.handleRawMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");

        std::string shutdownReq = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\",\"params\":null}";
        auto respOpt = server.handleRawMessage(shutdownReq);
        REQUIRE(respOpt.has_value());

        keet::json::detail::Parser p(*respOpt);
        auto jResp = p.parse_document();
        REQUIRE(jResp.is_object());
        const auto& respObj = std::get<JsonValue::Object>(jResp.value);
        REQUIRE(std::get<int64_t>(respObj.at("id").value) == 2);
        REQUIRE(respObj.at("result").is_null());
        REQUIRE(server.isShutdown());

        std::string exitNotice = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":null}";
        (void)server.handleRawMessage(exitNotice);
    }
}
