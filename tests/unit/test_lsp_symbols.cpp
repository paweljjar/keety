#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <algorithm>
#include <sstream>

using namespace keety::lsp;

static const DocumentSymbol* findSymbol(const std::vector<DocumentSymbol>& list, const std::string& name) {
    auto it = std::find_if(list.begin(), list.end(), [&](const DocumentSymbol& s) {
        return s.name == name;
    });
    return it != list.end() ? &(*it) : nullptr;
}

TEST_CASE("Analyzer getDocumentSymbols extracts hierarchical document outline", "[lsp][symbols]") {
    Analyzer analyzer;

    SECTION("Hierarchical classes with fields, methods, and inheritance") {
        std::string code =
            "class Entity {\n"
            "    pub let id: int;\n"
            "}\n"
            "export class Player inherits Entity {\n"
            "    priv let health: int = 100;\n"
            "    pub const maxSpeed: float = 5.0;\n"
            "    pub async fn attack(power: int): void {}\n"
            "}\n";
        Document doc("file:///classes.keet", "keet", 1, code);

        auto symbols = analyzer.getDocumentSymbols(doc);
        REQUIRE(symbols.size() == 2);

        // 1. Entity class
        auto* entity = findSymbol(symbols, "Entity");
        REQUIRE(entity != nullptr);
        REQUIRE(entity->kind == SymbolKind::Class);
        REQUIRE(entity->children.size() == 1);
        REQUIRE(entity->children[0].name == "id");
        REQUIRE(entity->children[0].kind == SymbolKind::Field);

        // 2. Player class
        auto* player = findSymbol(symbols, "Player");
        REQUIRE(player != nullptr);
        REQUIRE(player->kind == SymbolKind::Class);
        REQUIRE(player->detail.find("inherits Entity") != std::string::npos);
        REQUIRE(player->children.size() == 3);

        auto* health = findSymbol(player->children, "health");
        REQUIRE(health != nullptr);
        REQUIRE(health->kind == SymbolKind::Field);
        REQUIRE(health->detail.find("health: int") != std::string::npos);

        auto* maxSpeed = findSymbol(player->children, "maxSpeed");
        REQUIRE(maxSpeed != nullptr);
        REQUIRE(maxSpeed->kind == SymbolKind::Constant);
        REQUIRE(maxSpeed->detail.find("const maxSpeed: float") != std::string::npos);

        auto* attack = findSymbol(player->children, "attack");
        REQUIRE(attack != nullptr);
        REQUIRE(attack->kind == SymbolKind::Method);
        REQUIRE(attack->detail.find("async fn attack(power: int): void") != std::string::npos);
    }

    SECTION("Top-level functions and exports") {
        std::string code =
            "fn add(a: int, b: int): int {\n"
            "    return a + b;\n"
            "}\n"
            "export async fn fetch(url: string): string {\n"
            "    return \"\";\n"
            "}\n";
        Document doc("file:///functions.keet", "keet", 1, code);

        auto symbols = analyzer.getDocumentSymbols(doc);
        REQUIRE(symbols.size() == 2);

        auto* addFn = findSymbol(symbols, "add");
        REQUIRE(addFn != nullptr);
        REQUIRE(addFn->kind == SymbolKind::Function);
        REQUIRE(addFn->detail == "fn add(a: int, b: int): int");
        REQUIRE(addFn->range.start.line == 0);
        REQUIRE(addFn->selectionRange.start.line == 0);

        auto* fetchFn = findSymbol(symbols, "fetch");
        REQUIRE(fetchFn != nullptr);
        REQUIRE(fetchFn->kind == SymbolKind::Function);
        REQUIRE(fetchFn->detail == "export async fn fetch(url: string): string");
    }

    SECTION("Enums with variant members") {
        std::string code =
            "enum Status {\n"
            "    Active = 1,\n"
            "    Inactive = 2,\n"
            "    Pending = 3\n"
            "}\n";
        Document doc("file:///enums.keet", "keet", 1, code);

        auto symbols = analyzer.getDocumentSymbols(doc);
        REQUIRE(symbols.size() == 1);

        auto* status = findSymbol(symbols, "Status");
        REQUIRE(status != nullptr);
        REQUIRE(status->kind == SymbolKind::Enum);
        REQUIRE(status->children.size() == 3);

        REQUIRE(status->children[0].name == "Active");
        REQUIRE(status->children[0].kind == SymbolKind::EnumMember);
        REQUIRE(status->children[1].name == "Inactive");
        REQUIRE(status->children[1].kind == SymbolKind::EnumMember);
        REQUIRE(status->children[2].name == "Pending");
        REQUIRE(status->children[2].kind == SymbolKind::EnumMember);
    }

    SECTION("Top-level variables, constants, defines, error types, and imports") {
        std::string code =
            "import io;\n"
            "let appName: string = \"KeetApp\";\n"
            "const MAX_USERS: int = 1000;\n"
            "define TIMEOUT = 30;\n"
            "errtype NetworkError {\n"
            "    pub let code: int;\n"
            "}\n";
        Document doc("file:///declarations.keet", "keet", 1, code);

        auto symbols = analyzer.getDocumentSymbols(doc);
        REQUIRE(symbols.size() == 5);

        // Import
        auto* imp = findSymbol(symbols, "io");
        REQUIRE(imp != nullptr);
        REQUIRE(imp->kind == SymbolKind::Module);

        // Variable
        auto* var = findSymbol(symbols, "appName");
        REQUIRE(var != nullptr);
        REQUIRE(var->kind == SymbolKind::Variable);
        REQUIRE(var->detail.find("let appName: string") != std::string::npos);

        // Constant
        auto* cst = findSymbol(symbols, "MAX_USERS");
        REQUIRE(cst != nullptr);
        REQUIRE(cst->kind == SymbolKind::Constant);
        REQUIRE(cst->detail.find("const MAX_USERS: int") != std::string::npos);

        // Define
        auto* def = findSymbol(symbols, "TIMEOUT");
        REQUIRE(def != nullptr);
        REQUIRE(def->kind == SymbolKind::Constant);

        // ErrType
        auto* err = findSymbol(symbols, "NetworkError");
        REQUIRE(err != nullptr);
        REQUIRE(err->kind == SymbolKind::Struct);
        REQUIRE(err->children.size() == 1);
        REQUIRE(err->children[0].name == "code");
        REQUIRE(err->children[0].kind == SymbolKind::Field);
    }
}

TEST_CASE("Server handles textDocument/documentSymbol JSON-RPC request", "[lsp][server][symbols]") {
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

    // Verify documentSymbolProvider capability is advertised as true
    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    REQUIRE(jInit.is_object());
    const auto& initObj = std::get<JsonValue::Object>(jInit.value);
    const auto& resObj = std::get<JsonValue::Object>(initObj.at("result").value);
    const auto& capsObj = std::get<JsonValue::Object>(resObj.at("capabilities").value);
    REQUIRE(std::get<bool>(capsObj.at("documentSymbolProvider").value) == true);

    // 2. Open document with a class and functions
    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "  \"textDocument\": {"
        "    \"uri\": \"file:///server_symbols.keet\","
        "    \"languageId\": \"keet\","
        "    \"version\": 1,"
        "    \"text\": \"class Counter { pub let val: int; pub fn inc(): void {} }\\nfn main(): void {}\\n\""
        "  }"
        "}}";
    server.handleRawMessage(didOpen);

    // 3. Request document symbols
    std::string symReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/documentSymbol\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///server_symbols.keet\"}"
        "}}";
    auto symRespOpt = server.handleRawMessage(symReq);
    REQUIRE(symRespOpt.has_value());

    keet::json::detail::Parser pSym(*symRespOpt);
    auto jSym = pSym.parse_document();
    REQUIRE(jSym.is_object());
    const auto& symRespObj = std::get<JsonValue::Object>(jSym.value);
    REQUIRE(std::get<int64_t>(symRespObj.at("id").value) == 2);

    const auto& symArr = std::get<JsonValue::Array>(symRespObj.at("result").value);
    REQUIRE(symArr.size() == 2);

    // Counter class
    const auto& counterObj = std::get<JsonValue::Object>(symArr[0].value);
    REQUIRE(std::get<std::string>(counterObj.at("name").value) == "Counter");
    REQUIRE(std::get<int64_t>(counterObj.at("kind").value) == static_cast<int64_t>(SymbolKind::Class));
    REQUIRE(counterObj.find("children") != counterObj.end());

    const auto& counterChildren = std::get<JsonValue::Array>(counterObj.at("children").value);
    REQUIRE(counterChildren.size() == 2);

    // main function
    const auto& mainObj = std::get<JsonValue::Object>(symArr[1].value);
    REQUIRE(std::get<std::string>(mainObj.at("name").value) == "main");
    REQUIRE(std::get<int64_t>(mainObj.at("kind").value) == static_cast<int64_t>(SymbolKind::Function));

    // 4. Request document symbols on unopened document returns empty array
    std::string unopenReq =
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/documentSymbol\",\"params\":{"
        "  \"textDocument\": {\"uri\": \"file:///nonexistent.keet\"}"
        "}}";
    auto unopenRespOpt = server.handleRawMessage(unopenReq);
    REQUIRE(unopenRespOpt.has_value());

    keet::json::detail::Parser pUnopen(*unopenRespOpt);
    auto jUnopen = pUnopen.parse_document();
    const auto& unopenObj = std::get<JsonValue::Object>(jUnopen.value);
    const auto& unopenArr = std::get<JsonValue::Array>(unopenObj.at("result").value);
    REQUIRE(unopenArr.empty());
}
