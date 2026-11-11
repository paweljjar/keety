#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer prepareRename validates renameable symbols", "[lsp][rename]") {
    Analyzer analyzer;

    std::string code =
        "fn calculate(factor: int): int {\n" // line 0
        "    let count = 100;\n"              // line 1
        "    return count * factor;\n"        // line 2
        "}\n";                                // line 3

    Document doc("file:///prepare.keet", "keet", 1, code);

    SECTION("Validates local variable under cursor") {
        // Cursor on 'count' at line 1, col 9
        auto prep = analyzer.prepareRename(doc, Position{1, 9});
        REQUIRE(prep.has_value());
        CHECK(prep->placeholder == "count");
        CHECK(prep->range.start.line == 1);
        CHECK(prep->range.start.character == 8);
        CHECK(prep->range.end.character == 13);
    }

    SECTION("Validates function parameter under cursor") {
        // Cursor on 'factor' at line 0, col 15
        auto prep = analyzer.prepareRename(doc, Position{0, 15});
        REQUIRE(prep.has_value());
        CHECK(prep->placeholder == "factor");
        CHECK(prep->range.start.line == 0);
    }

    SECTION("Validates function declaration under cursor") {
        // Cursor on 'calculate' at line 0, col 5
        auto prep = analyzer.prepareRename(doc, Position{0, 5});
        REQUIRE(prep.has_value());
        CHECK(prep->placeholder == "calculate");
        CHECK(prep->range.start.line == 0);
    }

    SECTION("Rejects keywords, built-ins, and literals") {
        // Keyword 'fn': line 0, col 0
        CHECK_FALSE(analyzer.prepareRename(doc, Position{0, 0}).has_value());
        // Keyword 'let': line 1, col 5
        CHECK_FALSE(analyzer.prepareRename(doc, Position{1, 5}).has_value());
        // Keyword 'return': line 2, col 5
        CHECK_FALSE(analyzer.prepareRename(doc, Position{2, 5}).has_value());
        // Built-in type 'int': line 0, col 23
        CHECK_FALSE(analyzer.prepareRename(doc, Position{0, 23}).has_value());
        // Integer literal '100': line 1, col 17
        CHECK_FALSE(analyzer.prepareRename(doc, Position{1, 17}).has_value());
        // Whitespace
        CHECK_FALSE(analyzer.prepareRename(doc, Position{3, 0}).has_value());
    }
}

TEST_CASE("Analyzer renameSymbol produces scope-safe WorkspaceEdit", "[lsp][rename]") {
    Analyzer analyzer;

    SECTION("Renames local variable across all usages in function") {
        std::string code =
            "fn compute(): int {\n"       // line 0
            "    let total = 10;\n"       // line 1
            "    total = total + 5;\n"    // line 2
            "    return total;\n"         // line 3
            "}\n";                        // line 4

        Document doc("file:///rename_local.keet", "keet", 1, code);

        // Rename 'total' to 'accumulated'
        auto editOpt = analyzer.renameSymbol(doc, Position{1, 9}, "accumulated");
        REQUIRE(editOpt.has_value());
        REQUIRE(editOpt->changes.find("file:///rename_local.keet") != editOpt->changes.end());

        const auto& edits = editOpt->changes.at("file:///rename_local.keet");
        REQUIRE(edits.size() == 4);
        for (const auto& edit : edits) {
            CHECK(edit.newText == "accumulated");
        }
        CHECK(edits[0].range.start.line == 1);
        CHECK(edits[1].range.start.line == 2);
        CHECK(edits[2].range.start.line == 2);
        CHECK(edits[3].range.start.line == 3);
    }

    SECTION("Does not rename shadowed or sibling local variables") {
        std::string code =
            "fn funcA(): void {\n"       // line 0
            "    let x = 1;\n"           // line 1
            "    let a = x + 1;\n"       // line 2
            "}\n"                        // line 3
            "fn funcB(): void {\n"       // line 4
            "    let x = 2;\n"           // line 5
            "    let b = x + 2;\n"       // line 6
            "}\n";                       // line 7

        Document doc("file:///isolated.keet", "keet", 1, code);

        // Rename 'x' in funcA to 'firstVal'
        auto editOpt = analyzer.renameSymbol(doc, Position{1, 8}, "firstVal");
        REQUIRE(editOpt.has_value());
        const auto& edits = editOpt->changes.at("file:///isolated.keet");
        REQUIRE(edits.size() == 2);
        CHECK(edits[0].range.start.line == 1);
        CHECK(edits[1].range.start.line == 2);
    }

    SECTION("Rejects invalid identifiers and keywords as newName") {
        std::string code =
            "fn run(): void {\n"
            "    let val = 42;\n"
            "}\n";

        Document doc("file:///invalid.keet", "keet", 1, code);

        // Keyword
        CHECK_FALSE(analyzer.renameSymbol(doc, Position{1, 8}, "fn").has_value());
        CHECK_FALSE(analyzer.renameSymbol(doc, Position{1, 8}, "while").has_value());
        // Malformed identifiers
        CHECK_FALSE(analyzer.renameSymbol(doc, Position{1, 8}, "123num").has_value());
        CHECK_FALSE(analyzer.renameSymbol(doc, Position{1, 8}, "my-val").has_value());
        CHECK_FALSE(analyzer.renameSymbol(doc, Position{1, 8}, "").has_value());
    }

    SECTION("Rejects renaming when newName collides with existing variable in same scope") {
        std::string code =
            "fn process(): void {\n"
            "    let a = 1;\n"
            "    let b = 2;\n"
            "}\n";

        Document doc("file:///collision.keet", "keet", 1, code);

        // Attempt to rename 'a' to 'b' which already exists in process()
        auto editOpt = analyzer.renameSymbol(doc, Position{1, 8}, "b");
        CHECK_FALSE(editOpt.has_value());
    }
}

TEST_CASE("Analyzer renameSymbol across multiple open workspace documents", "[lsp][rename]") {
    Analyzer analyzer;
    DocumentManager docMgr;

    std::string utilsCode =
        "export fn calculate(val: int): int {\n" // line 0
        "    return val * 10;\n"                 // line 1
        "}\n";                                   // line 2

    std::string mainCode =
        "from \"./utils.keet\" import calculate;\n" // line 0
        "fn run(): int {\n"                         // line 1
        "    return calculate(5);\n"                // line 2
        "}\n";                                      // line 3

    docMgr.openDocument(TextDocumentItem{"file:///utils.keet", "keet", 1, utilsCode});
    docMgr.openDocument(TextDocumentItem{"file:///main.keet", "keet", 1, mainCode});

    auto utilsDoc = docMgr.getDocument("file:///utils.keet").value();
    auto mainDoc = docMgr.getDocument("file:///main.keet").value();

    SECTION("Renaming from declaration in utils.keet updates all open documents") {
        auto editOpt = analyzer.renameSymbol(utilsDoc, Position{0, 12}, "computeVal", &docMgr);
        REQUIRE(editOpt.has_value());
        REQUIRE(editOpt->changes.size() == 2);

        // Check utils.keet edit
        REQUIRE(editOpt->changes.find("file:///utils.keet") != editOpt->changes.end());
        const auto& utilsEdits = editOpt->changes.at("file:///utils.keet");
        REQUIRE(utilsEdits.size() == 1);
        CHECK(utilsEdits[0].range.start.line == 0);
        CHECK(utilsEdits[0].newText == "computeVal");

        // Check main.keet edits
        REQUIRE(editOpt->changes.find("file:///main.keet") != editOpt->changes.end());
        const auto& mainEdits = editOpt->changes.at("file:///main.keet");
        REQUIRE(mainEdits.size() == 2);
        CHECK(mainEdits[0].range.start.line == 0); // import site
        CHECK(mainEdits[1].range.start.line == 2); // call site
        CHECK(mainEdits[0].newText == "computeVal");
        CHECK(mainEdits[1].newText == "computeVal");
    }

    SECTION("Renaming from call site in main.keet updates all open documents") {
        auto editOpt = analyzer.renameSymbol(mainDoc, Position{2, 14}, "computeVal", &docMgr);
        REQUIRE(editOpt.has_value());
        REQUIRE(editOpt->changes.size() == 2);
        CHECK(editOpt->changes.at("file:///utils.keet").size() == 1);
        CHECK(editOpt->changes.at("file:///main.keet").size() == 2);
    }
}

TEST_CASE("LSP Server textDocument/prepareRename and rename protocol integration", "[lsp][server][rename]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    // 1. Initialize
    std::string initReq = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initResp = server.handleRawMessage(initReq);
    REQUIRE(initResp.has_value());

    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    const auto& resMap = std::get<JsonValue::Object>(std::get<JsonValue::Object>(jInit.value).at("result").value);
    const auto& caps = std::get<JsonValue::Object>(resMap.at("capabilities").value);
    REQUIRE(caps.find("renameProvider") != caps.end());
    const auto& renProv = std::get<JsonValue::Object>(caps.at("renameProvider").value);
    CHECK(std::get<bool>(renProv.at("prepareProvider").value) == true);

    // 2. Open document
    std::string code =
        "fn test(): int {\n"
        "    let count = 1;\n"
        "    return count;\n"
        "}\n";
    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.keet\",\"languageId\":\"keet\",\"version\":1,\"text\":\"" +
        std::string("fn test(): int {\\n    let count = 1;\\n    return count;\\n}\\n") +
        "\"}}}";
    server.handleRawMessage(didOpen);

    // 3. prepareRename request
    std::string prepReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/prepareRename\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.keet\"},"
        "\"position\":{\"line\":1,\"character\":9}}}";
    auto prepResp = server.handleRawMessage(prepReq);
    REQUIRE(prepResp.has_value());

    keet::json::detail::Parser pPrep(*prepResp);
    auto jPrep = pPrep.parse_document();
    const auto& prepObj = std::get<JsonValue::Object>(jPrep.value);
    REQUIRE(std::get<int64_t>(prepObj.at("id").value) == 2);
    const auto& prepRes = std::get<JsonValue::Object>(prepObj.at("result").value);
    CHECK(std::get<std::string>(prepRes.at("placeholder").value) == "count");

    // 4. rename request
    std::string renReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/rename\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.keet\"},"
        "\"position\":{\"line\":1,\"character\":9},"
        "\"newName\":\"counter\"}}";
    auto renResp = server.handleRawMessage(renReq);
    REQUIRE(renResp.has_value());

    keet::json::detail::Parser pRen(*renResp);
    auto jRen = pRen.parse_document();
    const auto& renObj = std::get<JsonValue::Object>(jRen.value);
    REQUIRE(std::get<int64_t>(renObj.at("id").value) == 3);
    const auto& renRes = std::get<JsonValue::Object>(renObj.at("result").value);
    const auto& changes = std::get<JsonValue::Object>(renRes.at("changes").value);
    REQUIRE(changes.find("file:///test.keet") != changes.end());
    const auto& editsArr = std::get<JsonValue::Array>(changes.at("file:///test.keet").value);
    REQUIRE(editsArr.size() == 2);
}
