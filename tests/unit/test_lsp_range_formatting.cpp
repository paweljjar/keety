#include <catch2/catch_test_macros.hpp>
#include <keety/fmt/formatter.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Formatter formatRange formats only specified line range", "[fmt][range_formatting]") {
    std::string unformatted =
        "fn first(): void {\n"
        "    let a: int = 1+2;\n"     // line 1
        "    let b: int = 3*4;\n"     // line 2
        "    let c: int = 5+6;\n"     // line 3
        "}\n";

    keety::fmt::Formatter formatter;
    // Format only lines 1 to 2
    std::string result = formatter.formatRange(unformatted, 1, 2);

    // Line 1 and 2 should be formatted
    CHECK(result.find("    let a: int = 1 + 2;") != std::string::npos);
    CHECK(result.find("    let b: int = 3 * 4;") != std::string::npos);
    // Line 3 should remain unformatted
    CHECK(result.find("    let c: int = 5+6;") != std::string::npos);
}

TEST_CASE("Analyzer formatRange generates selective TextEdits", "[lsp][range_formatting]") {
    Analyzer analyzer;

    std::string code =
        "fn test(): void {\n"
        "    let a: int = 1+2;\n"      // line 1
        "    let b: int = 3*4;\n"      // line 2
        "    let c: int = 5+6;\n"      // line 3
        "}\n";                         // line 4

    Document doc("file:///range_fmt.keet", "keet", 1, code);

    SECTION("Formats only line 1") {
        Range r{Position{1, 0}, Position{1, 21}};
        auto edits = analyzer.formatRange(doc, r);

        REQUIRE(edits.size() == 1);
        CHECK(edits[0].range.start.line == 1);
        CHECK(edits[0].newText == "    let a: int = 1 + 2;\n");
    }

    SECTION("Formats lines 1 to 2, leaving line 3 untouched") {
        Range r{Position{1, 0}, Position{2, 21}};
        auto edits = analyzer.formatRange(doc, r);

        REQUIRE_FALSE(edits.empty());
        for (const auto& edit : edits) {
            // No edit should touch line 3 or line 0
            CHECK(edit.range.start.line >= 1);
            CHECK(edit.range.start.line <= 2);
        }
    }
}

TEST_CASE("Analyzer formatRange handles multi-line blocks with nested indentation", "[lsp][range_formatting]") {
    Analyzer analyzer;

    std::string code =
        "fn run(x: int): int {\n"       // line 0
        "    if(x>0){\n"                // line 1
        "    return x;\n"               // line 2
        "}\n"                           // line 3
        "    let unformatted: int=10;\n" // line 4
        "    return 0;\n"
        "}\n";

    Document doc("file:///nested_range_fmt.keet", "keet", 1, code);

    // Format lines 1 to 3 (the if-block)
    Range r{Position{1, 0}, Position{3, 6}};
    auto edits = analyzer.formatRange(doc, r);

    REQUIRE_FALSE(edits.empty());
    std::string joinedNewText;
    for (const auto& edit : edits) {
        CHECK(edit.range.start.line >= 1);
        CHECK(edit.range.start.line <= 3);
        joinedNewText += edit.newText;
    }

    // Verify 4-space and 8-space indentation inside the formatted if-block
    CHECK(joinedNewText.find("    if (x > 0) {\n") != std::string::npos);
    CHECK(joinedNewText.find("        return x;\n") != std::string::npos);
    CHECK(joinedNewText.find("    }\n") != std::string::npos);
}

TEST_CASE("Analyzer formatRange edge cases", "[lsp][range_formatting]") {
    Analyzer analyzer;

    SECTION("Already formatted range returns empty edits") {
        std::string code =
            "fn clean(): void {\n"
            "    let a: int = 1;\n"
            "}\n";
        Document doc("file:///clean.keet", "keet", 1, code);
        Range r{Position{1, 0}, Position{1, 19}};
        auto edits = analyzer.formatRange(doc, r);
        CHECK(edits.empty());
    }

    SECTION("Document with syntax errors does not emit destructive edits") {
        std::string code =
            "fn broken( {\n"
            "    let a: int = 1;\n"
            "}\n";
        Document doc("file:///broken.keet", "keet", 1, code);
        Range r{Position{1, 0}, Position{1, 19}};
        auto edits = analyzer.formatRange(doc, r);
        CHECK(edits.empty());
    }
}

TEST_CASE("LSP Server textDocument/rangeFormatting protocol integration", "[lsp][range_formatting]") {
    std::string in = "";
    std::ostringstream out;
    std::istringstream inStream(in);
    Transport transport(inStream, out);
    Server server(transport);

    // 1. Initialize server and verify documentRangeFormattingProvider capability
    std::string initReq =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initResp = server.handleRawMessage(initReq);
    REQUIRE(initResp.has_value());

    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    const auto& initObj = std::get<JsonValue::Object>(jInit.value);
    const auto& resObj = std::get<JsonValue::Object>(initObj.at("result").value);
    const auto& capsObj = std::get<JsonValue::Object>(resObj.at("capabilities").value);
    REQUIRE(capsObj.find("documentRangeFormattingProvider") != capsObj.end());
    CHECK(std::get<bool>(capsObj.at("documentRangeFormattingProvider").value) == true);

    // 2. Open document
    std::string docContent =
        "fn test(): void {\\n"
        "    let a: int = 1+2;\\n"
        "    let b: int = 3*4;\\n"
        "}\\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test_range_fmt.keet\",\"languageId\":\"keet\",\"version\":1,"
        "\"text\":\"" + docContent + "\"}}}";
    server.handleRawMessage(didOpen);

    // 3. Request rangeFormatting for line 1
    std::string rangeFmtReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/rangeFormatting\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test_range_fmt.keet\"},"
        "\"range\":{\"start\":{\"line\":1,\"character\":0},\"end\":{\"line\":1,\"character\":21}},"
        "\"options\":{\"tabSize\":4,\"insertSpaces\":true}}}";
    auto rangeFmtResp = server.handleRawMessage(rangeFmtReq);
    REQUIRE(rangeFmtResp.has_value());

    keet::json::detail::Parser pFmt(*rangeFmtResp);
    auto jFmt = pFmt.parse_document();
    const auto& fmtObj = std::get<JsonValue::Object>(jFmt.value);
    REQUIRE(std::get<int64_t>(fmtObj.at("id").value) == 2);

    const auto& fResult = std::get<JsonValue::Array>(fmtObj.at("result").value);
    REQUIRE(fResult.size() == 1);
    const auto& editObj = std::get<JsonValue::Object>(fResult[0].value);
    const auto& editRange = std::get<JsonValue::Object>(editObj.at("range").value);
    const auto& rStart = std::get<JsonValue::Object>(editRange.at("start").value);
    CHECK(static_cast<uint32_t>(std::get<int64_t>(rStart.at("line").value)) == 1);
    CHECK(std::get<std::string>(editObj.at("newText").value) == "    let a: int = 1 + 2;\n");
}
