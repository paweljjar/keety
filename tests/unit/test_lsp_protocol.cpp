#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("LSP Protocol models and serialization", "[lsp][protocol]") {
    SECTION("Position and Range serialization") {
        Position pos{3, 14};
        auto jPos = pos.toJson();
        REQUIRE(jPos.is_object());
        const auto& obj = std::get<JsonValue::Object>(jPos.value);
        REQUIRE(std::get<int64_t>(obj.at("line").value) == 3);
        REQUIRE(std::get<int64_t>(obj.at("character").value) == 14);

        Position backPos = Position::fromJson(jPos);
        REQUIRE(backPos == pos);

        Range range{pos, Position{5, 20}};
        auto jRange = range.toJson();
        Range backRange = Range::fromJson(jRange);
        REQUIRE(backRange == range);
    }

    SECTION("TextDocumentItem and TextDocumentContentChangeEvent deserialization") {
        JsonValue::Object itemObj;
        itemObj["uri"] = JsonValue(std::string("file:///test.keet"));
        itemObj["languageId"] = JsonValue(std::string("keet"));
        itemObj["version"] = JsonValue(int64_t(42));
        itemObj["text"] = JsonValue(std::string("import io;\nfn main(): void {}\n"));

        auto item = TextDocumentItem::fromJson(JsonValue(std::move(itemObj)));
        REQUIRE(item.uri == "file:///test.keet");
        REQUIRE(item.languageId == "keet");
        REQUIRE(item.version == 42);
        REQUIRE(item.text.size() == 30);

        JsonValue::Object changeObj;
        changeObj["text"] = JsonValue(std::string("new content"));
        auto change = TextDocumentContentChangeEvent::fromJson(JsonValue(std::move(changeObj)));
        REQUIRE(!change.range.has_value());
        REQUIRE(change.text == "new content");
    }

    SECTION("JsonRpcResponse serialization") {
        JsonValue::Object resObj;
        resObj["status"] = JsonValue(std::string("ok"));

        auto success = JsonRpcResponse::makeResult(int64_t(1), JsonValue(std::move(resObj)));
        std::string raw = success.serialize();

        keet::json::detail::Parser p(raw);
        auto parsed = p.parse_document();
        REQUIRE(parsed.is_object());
        const auto& obj = std::get<JsonValue::Object>(parsed.value);
        REQUIRE(std::get<std::string>(obj.at("jsonrpc").value) == "2.0");
        REQUIRE(std::get<int64_t>(obj.at("id").value) == 1);
        const auto& resMap = std::get<JsonValue::Object>(obj.at("result").value);
        REQUIRE(std::get<std::string>(resMap.at("status").value) == "ok");

        auto err = JsonRpcResponse::makeError(
            std::string("req-123"),
            JsonRpcErrorCode::MethodNotFound,
            "No such method"
        );
        std::string rawErr = err.serialize();
        keet::json::detail::Parser pErr(rawErr);
        auto parsedErr = pErr.parse_document();
        const auto& objErr = std::get<JsonValue::Object>(parsedErr.value);
        REQUIRE(std::get<std::string>(objErr.at("id").value) == "req-123");
        const auto& errMap = std::get<JsonValue::Object>(objErr.at("error").value);
        REQUIRE(std::get<int64_t>(errMap.at("code").value) == -32601);
        REQUIRE(std::get<std::string>(errMap.at("message").value) == "No such method");
    }
}

TEST_CASE("Transport message framing and parsing", "[lsp][transport]") {
    SECTION("Frame message") {
        std::string msg = "{\"test\":true}";
        std::string framed = Transport::frameMessage(msg);
        REQUIRE(framed == "Content-Length: 13\r\n\r\n{\"test\":true}");
    }

    SECTION("Read message stream") {
        std::string payload1 = "{\"id\":1,\"method\":\"test\"}";
        std::string payload2 = "{\"id\":2,\"method\":\"next\"}";
        std::string input = Transport::frameMessage(payload1) + Transport::frameMessage(payload2);

        std::istringstream in(input);
        std::ostringstream out;
        Transport transport(in, out);

        auto msg1 = transport.readMessage();
        REQUIRE(msg1.has_value());
        REQUIRE(*msg1 == payload1);

        auto msg2 = transport.readMessage();
        REQUIRE(msg2.has_value());
        REQUIRE(*msg2 == payload2);

        auto msg3 = transport.readMessage();
        REQUIRE_FALSE(msg3.has_value()); // EOF
    }

    SECTION("Write message") {
        std::istringstream in("");
        std::ostringstream out;
        Transport transport(in, out);

        transport.writeMessage("{\"ok\":true}");
        REQUIRE(out.str() == "Content-Length: 11\r\n\r\n{\"ok\":true}");
    }
}
