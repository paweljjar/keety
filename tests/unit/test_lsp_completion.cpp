#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace keety::lsp;

static bool containsLabel(const CompletionList& list, const std::string& label) {
    return std::any_of(list.items.begin(), list.items.end(), [&](const CompletionItem& item) {
        return item.label == label;
    });
}

static const CompletionItem* findItem(const CompletionList& list, const std::string& label) {
    auto it = std::find_if(list.items.begin(), list.items.end(), [&](const CompletionItem& item) {
        return item.label == label;
    });
    return it != list.items.end() ? &(*it) : nullptr;
}

TEST_CASE("Analyzer getCompletions produces context-aware suggestions", "[lsp][completion]") {
    Analyzer analyzer;

    SECTION("General scope completions: keywords, snippets, types, stdlib modules") {
        std::string code = "fn main(): void {\n    \n}\n";
        Document doc("file:///main.keet", "keet", 1, code);

        // Cursor on empty line 1, col 4
        auto list = analyzer.getCompletions(doc, Position{1, 4});
        REQUIRE_FALSE(list.items.empty());

        // Keywords
        REQUIRE(containsLabel(list, "fn"));
        REQUIRE(containsLabel(list, "let"));
        REQUIRE(containsLabel(list, "mut"));
        REQUIRE(containsLabel(list, "const"));
        REQUIRE(containsLabel(list, "class"));
        REQUIRE(containsLabel(list, "zonk"));

        auto kwItem = findItem(list, "fn");
        REQUIRE(kwItem != nullptr);
        REQUIRE(kwItem->kind == CompletionItemKind::Keyword);

        // Snippets
        REQUIRE(containsLabel(list, "fn (snippet)"));
        REQUIRE(containsLabel(list, "for (snippet)"));
        REQUIRE(containsLabel(list, "if (snippet)"));
        REQUIRE(containsLabel(list, "main (snippet)"));

        auto snipItem = findItem(list, "fn (snippet)");
        REQUIRE(snipItem != nullptr);
        REQUIRE(snipItem->kind == CompletionItemKind::Snippet);
        REQUIRE(snipItem->insertTextFormat == InsertTextFormat::Snippet);
        REQUIRE(snipItem->insertText.find("fn ${1:name}") != std::string::npos);

        // Primitives
        REQUIRE(containsLabel(list, "int"));
        REQUIRE(containsLabel(list, "string"));
        REQUIRE(containsLabel(list, "bool"));
        REQUIRE(containsLabel(list, "float"));

        // Predeclared constants
        REQUIRE(containsLabel(list, "true"));
        REQUIRE(containsLabel(list, "false"));
        REQUIRE(containsLabel(list, "null"));

        // Stdlib modules
        REQUIRE(containsLabel(list, "io"));
        REQUIRE(containsLabel(list, "fs"));
        REQUIRE(containsLabel(list, "sys"));
        REQUIRE(containsLabel(list, "math"));
    }

    SECTION("Local function parameters and preceding local variables") {
        std::string code =
            "fn compute(baseVal: int, mut multiplier: int): int {\n"
            "    let offset = 42;\n"
            "    let ignoredAfter = 100;\n"
            "}\n";
        Document doc("file:///locals.keet", "keet", 1, code);

        // Cursor on line 2 (between offset and ignoredAfter), col 4
        // Line 2: "    let ignoredAfter = 100;\n" - before offset 60
        Position pos{2, 4};
        auto list = analyzer.getCompletions(doc, pos);

        // Parameters should be available
        auto p1 = findItem(list, "baseVal");
        REQUIRE(p1 != nullptr);
        REQUIRE(p1->kind == CompletionItemKind::Variable);
        REQUIRE(p1->detail.find("baseVal: int") != std::string::npos);

        auto p2 = findItem(list, "multiplier");
        REQUIRE(p2 != nullptr);
        REQUIRE(p2->kind == CompletionItemKind::Variable);
        REQUIRE(p2->detail.find("mut multiplier: int") != std::string::npos);

        // Local variable 'offset' defined on line 1 should be suggested
        auto v1 = findItem(list, "offset");
        REQUIRE(v1 != nullptr);
        REQUIRE(v1->kind == CompletionItemKind::Variable);
    }

    SECTION("Standard library module dot-completions") {
        // 1. io.
        std::string codeIo = "import io;\nfn main(): void {\n    io.\n}\n";
        Document docIo("file:///io_test.keet", "keet", 1, codeIo);
        auto listIo = analyzer.getCompletions(docIo, Position{2, 7});

        REQUIRE(containsLabel(listIo, "println"));
        REQUIRE(containsLabel(listIo, "print"));
        REQUIRE(containsLabel(listIo, "eprintln"));
        REQUIRE(containsLabel(listIo, "Color"));

        auto itemPrintln = findItem(listIo, "println");
        REQUIRE(itemPrintln != nullptr);
        REQUIRE(itemPrintln->kind == CompletionItemKind::Function);
        REQUIRE(itemPrintln->detail.find("println") != std::string::npos);
        REQUIRE(itemPrintln->documentation.find("standard output") != std::string::npos);

        // 2. math.
        std::string codeMath = "import math;\nfn test(): void {\n    math.\n}\n";
        Document docMath("file:///math_test.keet", "keet", 1, codeMath);
        auto listMath = analyzer.getCompletions(docMath, Position{2, 9});

        REQUIRE(containsLabel(listMath, "sqrt"));
        REQUIRE(containsLabel(listMath, "pow"));
        REQUIRE(containsLabel(listMath, "PI"));
        REQUIRE(containsLabel(listMath, "sin"));

        auto itemPi = findItem(listMath, "PI");
        REQUIRE(itemPi != nullptr);
        REQUIRE(itemPi->kind == CompletionItemKind::Constant);

        // 3. fs.
        std::string codeFs = "import fs;\nfn test(): void {\n    fs.\n}\n";
        Document docFs("file:///fs_test.keet", "keet", 1, codeFs);
        auto listFs = analyzer.getCompletions(docFs, Position{2, 7});

        REQUIRE(containsLabel(listFs, "readText"));
        REQUIRE(containsLabel(listFs, "writeText"));
        REQUIRE(containsLabel(listFs, "exists"));

        // 4. io.Color.
        std::string codeColor = "import io;\nfn test(): void {\n    io.Color.\n}\n";
        Document docColor("file:///color_test.keet", "keet", 1, codeColor);
        auto listColor = analyzer.getCompletions(docColor, Position{2, 13});

        REQUIRE(containsLabel(listColor, "Red"));
        REQUIRE(containsLabel(listColor, "Green"));
        REQUIRE(containsLabel(listColor, "Blue"));
        REQUIRE(containsLabel(listColor, "Default"));
    }

    SECTION("Class 'this.' and instance member dot-completion") {
        std::string code =
            "class Player {\n"
            "    pub let name: string;\n"
            "    pub let health: int;\n"
            "    pub fn heal(amount: int): void {\n"
            "        this.\n"
            "    }\n"
            "}\n"
            "fn main(): void {\n"
            "    let p = Player();\n"
            "    p.\n"
            "}\n";
        Document doc("file:///player.keet", "keet", 1, code);

        // 1. this. on line 4, col 13
        auto listThis = analyzer.getCompletions(doc, Position{4, 13});
        REQUIRE(containsLabel(listThis, "name"));
        REQUIRE(containsLabel(listThis, "health"));
        REQUIRE(containsLabel(listThis, "heal"));

        auto itemField = findItem(listThis, "health");
        REQUIRE(itemField != nullptr);
        REQUIRE(itemField->kind == CompletionItemKind::Field);

        auto itemMethod = findItem(listThis, "heal");
        REQUIRE(itemMethod != nullptr);
        REQUIRE(itemMethod->kind == CompletionItemKind::Method);

        // 2. p. on line 9, col 6
        auto listInst = analyzer.getCompletions(doc, Position{9, 6});
        REQUIRE(containsLabel(listInst, "name"));
        REQUIRE(containsLabel(listInst, "health"));
        REQUIRE(containsLabel(listInst, "heal"));
    }

    SECTION("Enum dot-completion") {
        std::string code =
            "enum Status {\n"
            "    Active = 1,\n"
            "    Pending = 2,\n"
            "    Archived = 3\n"
            "}\n"
            "fn test(): void {\n"
            "    Status.\n"
            "}\n";
        Document doc("file:///enum_test.keet", "keet", 1, code);

        auto listEnum = analyzer.getCompletions(doc, Position{6, 11});
        REQUIRE(containsLabel(listEnum, "Active"));
        REQUIRE(containsLabel(listEnum, "Pending"));
        REQUIRE(containsLabel(listEnum, "Archived"));

        auto itemActive = findItem(listEnum, "Active");
        REQUIRE(itemActive != nullptr);
        REQUIRE(itemActive->kind == CompletionItemKind::EnumMember);
    }

    SECTION("Relative import path completion from docManager and filesystem") {
        DocumentManager docManager;
        docManager.openDocument(TextDocumentItem{"file:///test_dir/main.keet", "keet", 1, "import \"./\n"});
        docManager.openDocument(TextDocumentItem{"file:///test_dir/helper.keet", "keet", 1, "export fn help(): void {}\n"});
        docManager.openDocument(TextDocumentItem{"file:///test_dir/utils.keet", "keet", 1, "export fn util(): void {}\n"});

        auto mainDoc = docManager.getDocument("file:///test_dir/main.keet");
        REQUIRE(mainDoc.has_value());

        // Cursor at line 0, col 10 (after import "./)
        auto list = analyzer.getCompletions(*mainDoc, Position{0, 10}, &docManager);
        REQUIRE_FALSE(list.items.empty());
        REQUIRE(containsLabel(list, "./helper"));
        REQUIRE(containsLabel(list, "./utils"));

        auto item = findItem(list, "./helper");
        REQUIRE(item != nullptr);
        REQUIRE(item->kind == CompletionItemKind::File);
    }
}

TEST_CASE("Server handles textDocument/completion JSON-RPC request", "[lsp][server][completion]") {
    std::istringstream input;
    std::ostringstream output;
    Transport transport(input, output);
    Server server(std::move(transport));

    // 1. Initialize
    std::string initReq =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{"
        "  \"processId\": 1234,"
        "  \"rootUri\": \"file:///workspace\","
        "  \"capabilities\": {}"
        "}}";
    auto initResp = server.handleRawMessage(initReq);
    REQUIRE(initResp.has_value());

    // Verify completionProvider capability is advertised with trigger characters
    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    REQUIRE(jInit.is_object());
    const auto& initObj = std::get<JsonValue::Object>(jInit.value);
    const auto& resObj = std::get<JsonValue::Object>(initObj.at("result").value);
    const auto& capsObj = std::get<JsonValue::Object>(resObj.at("capabilities").value);
    REQUIRE(capsObj.find("completionProvider") != capsObj.end());

    // 2. Open document
    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "  \"textDocument\": {"
        "    \"uri\": \"file:///server_completion.keet\","
        "    \"languageId\": \"keet\","
        "    \"version\": 1,"
        "    \"text\": \"import io;\\nfn main(): void {\\n    io.\\n}\\n\""
        "  }"
        "}}";
    server.handleRawMessage(didOpen);

    // 3. Request completion at io. (line 2, col 7)
    std::string compReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///server_completion.keet\"},"
        "  \"position\": {\"line\": 2, \"character\": 7}"
        "}}";
    auto compRespOpt = server.handleRawMessage(compReq);
    REQUIRE(compRespOpt.has_value());

    keet::json::detail::Parser pComp(*compRespOpt);
    auto jComp = pComp.parse_document();
    REQUIRE(jComp.is_object());
    const auto& compRespObj = std::get<JsonValue::Object>(jComp.value);
    REQUIRE(std::get<int64_t>(compRespObj.at("id").value) == 2);

    const auto& compResult = std::get<JsonValue::Object>(compRespObj.at("result").value);
    REQUIRE(compResult.find("items") != compResult.end());
    const auto& itemsArr = std::get<JsonValue::Array>(compResult.at("items").value);
    REQUIRE_FALSE(itemsArr.empty());

    bool foundPrintln = false;
    for (const auto& itemVal : itemsArr) {
        const auto& itemObj = std::get<JsonValue::Object>(itemVal.value);
        if (std::get<std::string>(itemObj.at("label").value) == "println") {
            foundPrintln = true;
            REQUIRE(std::get<int64_t>(itemObj.at("kind").value) == static_cast<int64_t>(CompletionItemKind::Function));
            REQUIRE(itemObj.find("detail") != itemObj.end());
            REQUIRE(itemObj.find("documentation") != itemObj.end());
            break;
        }
    }
    REQUIRE(foundPrintln);

    // 4. Request completion on unopened document returns empty completion list
    std::string unopenReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/completion\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///nonexistent.keet\"},"
        "  \"position\": {\"line\": 0, \"character\": 0}"
        "}}";
    auto unopenRespOpt = server.handleRawMessage(unopenReq);
    REQUIRE(unopenRespOpt.has_value());

    keet::json::detail::Parser pUnopen(*unopenRespOpt);
    auto jUnopen = pUnopen.parse_document();
    const auto& unopenObj = std::get<JsonValue::Object>(jUnopen.value);
    const auto& unopenResult = std::get<JsonValue::Object>(unopenObj.at("result").value);
    const auto& emptyItems = std::get<JsonValue::Array>(unopenResult.at("items").value);
    REQUIRE(emptyItems.empty());
}
