#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer getSignatureHelp for local and module functions", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "/// Calculates the scaled factor with an offset.\n"
        "fn calculate(factor: int, offset: int): int {\n"   // line 1
        "    return factor * 2 + offset;\n"                // line 2
        "}\n"                                              // line 3
        "fn main(): void {\n"                              // line 4
        "    let res1 = calculate();\n"                    // line 5: col 26 is between ( and )
        "    let res2 = calculate(10, );\n"                // line 6: col 29 is after comma
        "    let res3 = calculate(10, 20, 30);\n"          // line 7: col 33 is after 2nd comma
        "}\n";

    Document doc("file:///sig_help.keet", "keet", 1, code);

    SECTION("Active parameter 0 right after opening parenthesis") {
        // Line 5: 'calculate()' -> col 25 is right between '(' and ')'
        auto help = analyzer.getSignatureHelp(doc, Position{5, 25});
        REQUIRE(help.has_value());
        CHECK(help->signatures.size() == 1);
        CHECK(help->activeSignature == 0);
        CHECK(help->activeParameter == 0);

        const auto& sig = help->signatures[0];
        CHECK(sig.label == "fn calculate(factor: int, offset: int): int");
        REQUIRE(sig.parameters.size() == 2);
        CHECK(sig.parameters[0].label == "factor: int");
        CHECK(sig.parameters[1].label == "offset: int");
        REQUIRE(sig.documentation.has_value());
        CHECK(sig.documentation->value.find("Calculates the scaled factor with an offset.") != std::string::npos);
    }

    SECTION("Active parameter 1 after first comma") {
        // Line 6: 'calculate(10, )' -> col 29 is right after ', '
        auto help = analyzer.getSignatureHelp(doc, Position{6, 29});
        REQUIRE(help.has_value());
        CHECK(help->activeParameter == 1);
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].parameters.size() == 2);
    }

    SECTION("Active parameter tracks beyond declared parameters") {
        // Line 7: 'calculate(10, 20, 30)' -> col 33 is in third argument
        auto help = analyzer.getSignatureHelp(doc, Position{7, 33});
        REQUIRE(help.has_value());
        CHECK(help->activeParameter == 2);
    }
}

TEST_CASE("Analyzer getSignatureHelp for nested function calls", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "fn inner(a: int, b: int): int { return a + b; }\n"          // line 0
        "fn outer(first: int, second: int): int { return first; }\n" // line 1
        "fn test(): void {\n"                                        // line 2
        "    let r1 = outer(inner(10, 20), 30);\n"                   // line 3
        "    let r2 = outer(inner(10, 20), 30);\n"                   // line 4
        "}\n";

    Document doc("file:///nested.keet", "keet", 1, code);

    SECTION("Inner call signature when cursor is inside inner call") {
        // Line 3: 'outer(inner(10, 20), 30)' -> '10, |20' at col 29
        auto help = analyzer.getSignatureHelp(doc, Position{3, 29});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn inner(a: int, b: int): int");
        CHECK(help->activeParameter == 1);
    }

    SECTION("Outer call signature when cursor is after inner call completes") {
        // Line 4: 'outer(inner(10, 20), |30)' -> col 34 (after comma)
        auto help = analyzer.getSignatureHelp(doc, Position{4, 34});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn outer(first: int, second: int): int");
        CHECK(help->activeParameter == 1);
    }
}

TEST_CASE("Analyzer getSignatureHelp ignores delimiters in arrays and objects", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "fn process(items: array<int>, suffix: string): void {}\n" // line 0
        "fn run(): void {\n"                                       // line 1
        "    process([1, 2, 3], \"test\");\n"                      // line 2
        "    process([1, 2], \"test\");\n"                         // line 3
        "}\n";

    Document doc("file:///delims.keet", "keet", 1, code);

    SECTION("Array commas do not increment outer call parameter index") {
        // Line 2: 'process([1, 2, 3], |"test")' -> col 23
        auto help = analyzer.getSignatureHelp(doc, Position{2, 23});
        REQUIRE(help.has_value());
        CHECK(help->signatures[0].label.find("fn process(") != std::string::npos);
        CHECK(help->activeParameter == 1);
    }

    SECTION("Cursor inside array expression counts as first parameter of process") {
        // Line 3: 'process([1, |2], "test")' -> col 15
        auto help = analyzer.getSignatureHelp(doc, Position{3, 15});
        REQUIRE(help.has_value());
        CHECK(help->signatures[0].label.find("fn process(") != std::string::npos);
        CHECK(help->activeParameter == 0);
    }
}

TEST_CASE("Analyzer getSignatureHelp for classes and methods", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "class Point {\n"                                      // line 0
        "    let x: int;\n"                                    // line 1
        "    let y: int;\n"                                    // line 2
        "    fn move(dx: int, dy: int): void {\n"              // line 3
        "        this.x = this.x + dx;\n"                      // line 4
        "    }\n"                                              // line 5
        "    fn shift(): void {\n"                             // line 6
        "        this.move(1, 2);\n"                           // line 7
        "    }\n"                                              // line 8
        "}\n"                                                  // line 9
        "fn main(): void {\n"                                  // line 10
        "    let p = Point(10, 20);\n"                         // line 11
        "    p.move(5, 10);\n"                                 // line 12
        "}\n";

    Document doc("file:///class_sig.keet", "keet", 1, code);

    SECTION("Constructor signature help") {
        // Line 11: 'Point(10, |20)' -> col 22
        auto help = analyzer.getSignatureHelp(doc, Position{11, 22});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "Point(x: int, y: int)");
        REQUIRE(help->signatures[0].parameters.size() == 2);
        CHECK(help->signatures[0].parameters[0].label == "x: int");
        CHECK(help->signatures[0].parameters[1].label == "y: int");
        CHECK(help->activeParameter == 1);
    }

    SECTION("Method call on object instance") {
        // Line 12: 'p.move(5, |10)' -> col 13
        auto help = analyzer.getSignatureHelp(doc, Position{12, 13});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn move(dx: int, dy: int): void");
        CHECK(help->activeParameter == 1);
    }

    SECTION("Method call via this") {
        // Line 7: 'this.move(1, |2)' -> col 20
        auto help = analyzer.getSignatureHelp(doc, Position{7, 20});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn move(dx: int, dy: int): void");
        CHECK(help->activeParameter == 1);
    }
}

TEST_CASE("Analyzer getSignatureHelp for standard library functions", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "fn test(): void {\n"
        "    appendText(\"file.txt\", \"data\");\n" // line 1: col 28
        "    fs.readFile(\"file.txt\");\n"          // line 2: col 16
        "}\n";

    Document doc("file:///stdlib_sig.keet", "keet", 1, code);

    SECTION("Built-in appendText signature") {
        // Line 1: 'appendText("file.txt", |"data")' -> col 28
        auto help = analyzer.getSignatureHelp(doc, Position{1, 28});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn appendText(path: string, content: string): void");
        REQUIRE(help->signatures[0].parameters.size() == 2);
        CHECK(help->signatures[0].parameters[0].label == "path: string");
        CHECK(help->signatures[0].parameters[1].label == "content: string");
        CHECK(help->activeParameter == 1);
        REQUIRE(help->signatures[0].documentation.has_value());
        CHECK(help->signatures[0].documentation->value.find("Appends text to the end of a file") != std::string::npos);
    }

    SECTION("Module-qualified fs.readFile signature") {
        // Line 2: 'fs.readFile(|"file.txt")' -> col 16
        auto help = analyzer.getSignatureHelp(doc, Position{2, 16});
        REQUIRE(help.has_value());
        REQUIRE(help->signatures.size() == 1);
        CHECK(help->signatures[0].label == "fn readFile(path: string): string");
        CHECK(help->activeParameter == 0);
    }
}

TEST_CASE("Analyzer getSignatureHelp across imported documents", "[lsp][signature]") {
    Analyzer analyzer;
    DocumentManager docMgr;

    std::string utilsCode =
        "/// Multiplies a value by factor.\n"
        "export fn multiply(val: int, factor: int): int {\n"
        "    return val * factor;\n"
        "}\n";
    docMgr.openDocument(TextDocumentItem{"file:///utils.keet", "keet", 1, utilsCode});

    std::string mainCode =
        "from \"./utils.keet\" import multiply;\n" // line 0
        "fn main(): void {\n"                      // line 1
        "    let x = multiply(10, 20);\n"          // line 2: col 25
        "}\n";
    docMgr.openDocument(TextDocumentItem{"file:///main.keet", "keet", 1, mainCode});

    auto mainDoc = docMgr.getDocument("file:///main.keet");
    REQUIRE(mainDoc.has_value());

    auto help = analyzer.getSignatureHelp(*mainDoc, Position{2, 25}, &docMgr);
    REQUIRE(help.has_value());
    REQUIRE(help->signatures.size() == 1);
    CHECK(help->signatures[0].label == "export fn multiply(val: int, factor: int): int");
    REQUIRE(help->signatures[0].parameters.size() == 2);
    CHECK(help->signatures[0].parameters[0].label == "val: int");
    CHECK(help->signatures[0].parameters[1].label == "factor: int");
    CHECK(help->activeParameter == 1);
    REQUIRE(help->signatures[0].documentation.has_value());
    CHECK(help->signatures[0].documentation->value.find("Multiplies a value by factor.") != std::string::npos);
}

TEST_CASE("Analyzer getSignatureHelp negative cases", "[lsp][signature]") {
    Analyzer analyzer;

    std::string code =
        "fn foo(a: int): void {}\n"      // line 0
        "let x = 10;\n"                  // line 1
        "if (x > 5) {\n"                 // line 2
        "    unknownCall(1, 2);\n"       // line 3
        "}\n";

    Document doc("file:///negative.keet", "keet", 1, code);

    SECTION("Outside any call returns nullopt") {
        CHECK_FALSE(analyzer.getSignatureHelp(doc, Position{1, 5}).has_value());
    }

    SECTION("Inside control flow if-condition returns nullopt") {
        CHECK_FALSE(analyzer.getSignatureHelp(doc, Position{2, 7}).has_value());
    }

    SECTION("Inside function declaration returns nullopt") {
        CHECK_FALSE(analyzer.getSignatureHelp(doc, Position{0, 10}).has_value());
    }

    SECTION("Unresolved symbol returns nullopt") {
        CHECK_FALSE(analyzer.getSignatureHelp(doc, Position{3, 18}).has_value());
    }
}

TEST_CASE("LSP Server textDocument/signatureHelp protocol integration", "[lsp][signature]") {
    std::string in = "";
    std::ostringstream out;
    std::istringstream inStream(in);
    Transport transport(inStream, out);
    Server server(transport);

    // Initialize server
    std::string initReq =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initResp = server.handleRawMessage(initReq);
    REQUIRE(initResp.has_value());

    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    const auto& initObj = std::get<JsonValue::Object>(jInit.value);
    const auto& resObj = std::get<JsonValue::Object>(initObj.at("result").value);
    const auto& capsObj = std::get<JsonValue::Object>(resObj.at("capabilities").value);
    REQUIRE(capsObj.find("signatureHelpProvider") != capsObj.end());
    const auto& sigHelpProv = std::get<JsonValue::Object>(capsObj.at("signatureHelpProvider").value);
    const auto& trigArr = std::get<JsonValue::Array>(sigHelpProv.at("triggerCharacters").value);
    REQUIRE(trigArr.size() == 2);

    // Open document
    std::string docContent =
        "fn calculate(a: int, b: int): int { return a + b; }\\n"
        "fn main(): void {\\n"
        "    let x = calculate(10, );\\n"
        "}\\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.keet\",\"languageId\":\"keet\",\"version\":1,"
        "\"text\":\"" + docContent + "\"}}}";
    server.handleRawMessage(didOpen);

    // Request signature help at line 2, col 26 (after comma)
    std::string sigReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/signatureHelp\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test.keet\"},"
        "\"position\":{\"line\":2,\"character\":26}}}";
    auto sigResp = server.handleRawMessage(sigReq);
    REQUIRE(sigResp.has_value());

    keet::json::detail::Parser pSig(*sigResp);
    auto jSig = pSig.parse_document();
    const auto& sigObj = std::get<JsonValue::Object>(jSig.value);
    REQUIRE(std::get<int64_t>(sigObj.at("id").value) == 2);
    const auto& sResult = std::get<JsonValue::Object>(sigObj.at("result").value);
    const auto& sArr = std::get<JsonValue::Array>(sResult.at("signatures").value);
    REQUIRE(sArr.size() == 1);
    CHECK(static_cast<uint32_t>(std::get<int64_t>(sResult.at("activeParameter").value)) == 1);
}
