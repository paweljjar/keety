#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer findReferences for local variables and parameters", "[lsp][references]") {
    Analyzer analyzer;

    SECTION("Finds local variable references with and without declaration") {
        std::string code =
            "fn compute(): int {\n"       // line 0
            "    let total = 10;\n"       // line 1: 'total' at line 1, col 8..13
            "    total = total + 5;\n"    // line 2: 'total' at col 4..9 and col 12..17
            "    return total;\n"         // line 3: 'total' at col 11..16
            "}\n";                        // line 4

        Document doc("file:///test.keet", "keet", 1, code);

        // Cursor on 'total' usage at line 2, col 5
        auto refsWithDecl = analyzer.findReferences(doc, Position{2, 5}, true);
        REQUIRE(refsWithDecl.size() == 4);
        CHECK(refsWithDecl[0].range.start.line == 1); // declaration
        CHECK(refsWithDecl[1].range.start.line == 2); // assignment target
        CHECK(refsWithDecl[2].range.start.line == 2); // expression
        CHECK(refsWithDecl[3].range.start.line == 3); // return

        // Cursor on 'total' declaration at line 1, col 9 with includeDeclaration = false
        auto refsNoDecl = analyzer.findReferences(doc, Position{1, 9}, false);
        REQUIRE(refsNoDecl.size() == 3);
        CHECK(refsNoDecl[0].range.start.line == 2);
        CHECK(refsNoDecl[1].range.start.line == 2);
        CHECK(refsNoDecl[2].range.start.line == 3);
    }

    SECTION("Finds function parameter references") {
        std::string code =
            "fn multiply(multiplier: int, value: int): int {\n" // line 0
            "    let result = multiplier * value;\n"            // line 1
            "    return multiplier + result;\n"                 // line 2
            "}\n";                                              // line 3

        Document doc("file:///param.keet", "keet", 1, code);

        // Cursor on 'multiplier' in param list: line 0, col 14
        auto refsMultiplier = analyzer.findReferences(doc, Position{0, 14}, true);
        REQUIRE(refsMultiplier.size() == 3);
        CHECK(refsMultiplier[0].range.start.line == 0);
        CHECK(refsMultiplier[1].range.start.line == 1);
        CHECK(refsMultiplier[2].range.start.line == 2);

        // Cursor on 'value' inside function body: line 1, col 32
        auto refsValue = analyzer.findReferences(doc, Position{1, 32}, false);
        REQUIRE(refsValue.size() == 1);
        CHECK(refsValue[0].range.start.line == 1);
    }
}

TEST_CASE("Analyzer findReferences handles lexical scoping and shadowing", "[lsp][references]") {
    Analyzer analyzer;

    SECTION("Distinguishes local variables with same name in sibling functions") {
        std::string code =
            "fn funcA(): void {\n"       // line 0
            "    let x = 1;\n"           // line 1
            "    let y = x + 10;\n"      // line 2
            "}\n"                        // line 3
            "fn funcB(): void {\n"       // line 4
            "    let x = 99;\n"          // line 5
            "    let z = x * 2;\n"       // line 6
            "}\n";                       // line 7

        Document doc("file:///siblings.keet", "keet", 1, code);

        // Find references for 'x' in funcA: line 1, col 8
        auto refsA = analyzer.findReferences(doc, Position{1, 8}, true);
        REQUIRE(refsA.size() == 2);
        CHECK(refsA[0].range.start.line == 1);
        CHECK(refsA[1].range.start.line == 2);

        // Find references for 'x' in funcB: line 5, col 8
        auto refsB = analyzer.findReferences(doc, Position{5, 8}, true);
        REQUIRE(refsB.size() == 2);
        CHECK(refsB[0].range.start.line == 5);
        CHECK(refsB[1].range.start.line == 6);
    }

    SECTION("Lexical block shadowing ignores shadowed variable") {
        std::string code =
            "let globalCounter = 0;\n"             // line 0
            "fn runner(): void {\n"                // line 1
            "    let counter = globalCounter + 1;\n" // line 2
            "    return counter;\n"                // line 3
            "}\n";                                 // line 4

        Document doc("file:///shadow.keet", "keet", 1, code);

        // References to globalCounter at line 0, col 5
        auto refsGlobal = analyzer.findReferences(doc, Position{0, 5}, true);
        REQUIRE(refsGlobal.size() == 2);
        CHECK(refsGlobal[0].range.start.line == 0);
        CHECK(refsGlobal[1].range.start.line == 2);
    }
}

TEST_CASE("Analyzer findReferences for top-level declarations, classes, and enums", "[lsp][references]") {
    Analyzer analyzer;

    SECTION("Finds top-level function calls") {
        std::string code =
            "fn helper(): int {\n"             // line 0
            "    return 42;\n"                 // line 1
            "}\n"                              // line 2
            "fn first(): int {\n"              // line 3
            "    return helper();\n"           // line 4
            "}\n"                              // line 5
            "fn second(): int {\n"             // line 6
            "    return helper() + 1;\n"       // line 7
            "}\n";                             // line 8

        Document doc("file:///fns.keet", "keet", 1, code);

        // Cursor on 'helper' declaration: line 0, col 4
        auto refs = analyzer.findReferences(doc, Position{0, 4}, true);
        REQUIRE(refs.size() == 3);
        CHECK(refs[0].range.start.line == 0);
        CHECK(refs[1].range.start.line == 4);
        CHECK(refs[2].range.start.line == 7);
    }

    SECTION("Finds class fields and methods") {
        std::string code =
            "class Vector {\n"                 // line 0
            "    let x: int;\n"                // line 1: field 'x'
            "    fn getX(): int {\n"           // line 2: method 'getX'
            "        return this.x;\n"         // line 3: member 'x'
            "    }\n"                          // line 4
            "}\n"                              // line 5
            "fn test(): int {\n"               // line 6
            "    let v = Vector();\n"          // line 7
            "    return v.x + v.getX();\n"     // line 8: 'v.x' at col 13, 'v.getX()' at col 19
            "}\n";                             // line 9

        Document doc("file:///class.keet", "keet", 1, code);

        // References to field 'x': line 1, col 8
        auto refsField = analyzer.findReferences(doc, Position{1, 8}, true);
        REQUIRE(refsField.size() == 3);
        CHECK(refsField[0].range.start.line == 1);
        CHECK(refsField[1].range.start.line == 3);
        CHECK(refsField[2].range.start.line == 8);

        // References to method 'getX': line 2, col 8
        auto refsMethod = analyzer.findReferences(doc, Position{2, 8}, true);
        REQUIRE(refsMethod.size() == 2);
        CHECK(refsMethod[0].range.start.line == 2);
        CHECK(refsMethod[1].range.start.line == 8);
    }

    SECTION("Finds enum member references") {
        std::string code =
            "enum Color {\n"                   // line 0
            "    Red,\n"                       // line 1
            "    Green\n"                      // line 2
            "}\n"                              // line 3
            "fn isRed(c: Color): bool {\n"     // line 4
            "    return c == Color.Red;\n"     // line 5: 'Red' at col 22
            "}\n";                             // line 6

        Document doc("file:///enum.keet", "keet", 1, code);

        // References to 'Red': line 1, col 5
        auto refs = analyzer.findReferences(doc, Position{1, 5}, true);
        REQUIRE(refs.size() == 2);
        CHECK(refs[0].range.start.line == 1);
        CHECK(refs[1].range.start.line == 5);
    }
}

TEST_CASE("Analyzer findReferences across multiple open workspace documents", "[lsp][references]") {
    Analyzer analyzer;
    DocumentManager docMgr;

    std::string utilsCode =
        "export fn calculate(val: int): int {\n" // line 0
        "    return val * 10;\n"                 // line 1
        "}\n";                                   // line 2

    std::string mainCode =
        "from \"./utils.keet\" import calculate;\n"     // line 0
        "fn run(): int {\n"                             // line 1
        "    let a = calculate(5);\n"                   // line 2
        "    let b = calculate(20);\n"                  // line 3
        "    return a + b;\n"                           // line 4
        "}\n";                                          // line 5

    docMgr.openDocument(TextDocumentItem{"file:///utils.keet", "keet", 1, utilsCode});
    docMgr.openDocument(TextDocumentItem{"file:///main.keet", "keet", 1, mainCode});

    auto utilsDoc = docMgr.getDocument("file:///utils.keet").value();
    auto mainDoc = docMgr.getDocument("file:///main.keet").value();

    SECTION("Finds references starting from export in utils.keet") {
        auto refs = analyzer.findReferences(utilsDoc, Position{0, 12}, true, &docMgr);
        REQUIRE(refs.size() == 4);

        // 1 in utils.keet
        CHECK(refs[0].uri == "file:///main.keet");
        CHECK(refs[0].range.start.line == 0); // import site
        CHECK(refs[1].uri == "file:///main.keet");
        CHECK(refs[1].range.start.line == 2); // call 1
        CHECK(refs[2].uri == "file:///main.keet");
        CHECK(refs[2].range.start.line == 3); // call 2
        CHECK(refs[3].uri == "file:///utils.keet");
        CHECK(refs[3].range.start.line == 0); // declaration
    }

    SECTION("Finds references starting from imported call site in main.keet") {
        auto refs = analyzer.findReferences(mainDoc, Position{2, 14}, true, &docMgr);
        REQUIRE(refs.size() == 4);
        CHECK(refs[3].uri == "file:///utils.keet");
        CHECK(refs[3].range.start.line == 0);
    }
}

TEST_CASE("LSP Server textDocument/references protocol integration", "[lsp][server][references]") {
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
    REQUIRE(caps.find("referencesProvider") != caps.end());
    CHECK(std::get<bool>(caps.at("referencesProvider").value) == true);

    // 2. Open document
    std::string code =
        "fn greet(name: string): string {\n" // line 0
        "    let msg = \"Hello \" + name;\n" // line 1
        "    return msg;\n"                  // line 2
        "}\n";
    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///greet.keet\",\"languageId\":\"keet\",\"version\":1,\"text\":\"" +
        std::string("fn greet(name: string): string {\\n    let msg = \\\"Hello \\\" + name;\\n    return msg;\\n}\\n") +
        "\"}}}";
    server.handleRawMessage(didOpen);

    // 3. Request references for 'msg' with includeDeclaration = true
    std::string refReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/references\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///greet.keet\"},"
        "\"position\":{\"line\":2,\"character\":12},"
        "\"context\":{\"includeDeclaration\":true}}}";
    auto refResp = server.handleRawMessage(refReq);
    REQUIRE(refResp.has_value());

    keet::json::detail::Parser pRef(*refResp);
    auto jRef = pRef.parse_document();
    const auto& refObj = std::get<JsonValue::Object>(jRef.value);
    REQUIRE(std::get<int64_t>(refObj.at("id").value) == 2);
    const auto& resultsArr = std::get<JsonValue::Array>(refObj.at("result").value);
    REQUIRE(resultsArr.size() == 2); // declaration at line 1, return at line 2
}
