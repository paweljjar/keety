#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>
#include <vector>

using namespace keety::lsp;

namespace {

struct DecodedToken {
    uint32_t line;
    uint32_t character;
    uint32_t length;
    uint32_t tokenType;
    uint32_t tokenModifiers;
};

std::vector<DecodedToken> decodeSemanticTokens(const std::vector<uint32_t>& data) {
    REQUIRE(data.size() % 5 == 0);
    std::vector<DecodedToken> tokens;
    tokens.reserve(data.size() / 5);

    uint32_t curLine = 0;
    uint32_t curChar = 0;

    for (size_t i = 0; i < data.size(); i += 5) {
        uint32_t deltaLine = data[i];
        uint32_t deltaChar = data[i + 1];
        uint32_t length = data[i + 2];
        uint32_t tokenType = data[i + 3];
        uint32_t tokenModifiers = data[i + 4];

        curLine += deltaLine;
        if (deltaLine == 0) {
            curChar += deltaChar;
        } else {
            curChar = deltaChar;
        }

        tokens.push_back(DecodedToken{curLine, curChar, length, tokenType, tokenModifiers});
    }

    return tokens;
}

// Legend indices
constexpr uint32_t SemType = 0;
constexpr uint32_t SemClass = 1;
constexpr uint32_t SemEnum = 3;
constexpr uint32_t SemEnumMember = 4;
constexpr uint32_t SemFunction = 5;
constexpr uint32_t SemMethod = 6;
constexpr uint32_t SemProperty = 7;
constexpr uint32_t SemVariable = 8;
constexpr uint32_t SemParameter = 9;
constexpr uint32_t SemKeyword = 10;
constexpr uint32_t SemString = 11;
constexpr uint32_t SemNumber = 12;
constexpr uint32_t SemOperator = 13;
constexpr uint32_t SemComment = 14;

// Modifiers
constexpr uint32_t ModDeclaration = 1 << 0;
constexpr uint32_t ModReadonly = 1 << 1;
constexpr uint32_t ModDefaultLibrary = 1 << 3;

} // namespace

TEST_CASE("Analyzer semantic tokens for functions and parameters", "[lsp][semantic_tokens]") {
    Analyzer analyzer;

    std::string code =
        "fn calculate(factor: int, offset: int): int {\n"
        "    return factor * 2 + offset;\n"
        "}\n";

    Document doc("file:///fn_tokens.keet", "keet", 1, code);
    auto semTokens = analyzer.getSemanticTokens(doc);
    auto decoded = decodeSemanticTokens(semTokens.data);

    REQUIRE_FALSE(decoded.empty());

    // Token 0: 'fn' -> Keyword
    CHECK(decoded[0].line == 0);
    CHECK(decoded[0].character == 0);
    CHECK(decoded[0].length == 2);
    CHECK(decoded[0].tokenType == SemKeyword);

    // Token 1: 'calculate' -> Function, Declaration
    CHECK(decoded[1].line == 0);
    CHECK(decoded[1].character == 3);
    CHECK(decoded[1].length == 9);
    CHECK(decoded[1].tokenType == SemFunction);
    CHECK((decoded[1].tokenModifiers & ModDeclaration) != 0);

    // Token 2: 'factor' -> Parameter, Declaration
    CHECK(decoded[2].line == 0);
    CHECK(decoded[2].character == 13);
    CHECK(decoded[2].length == 6);
    CHECK(decoded[2].tokenType == SemParameter);
    CHECK((decoded[2].tokenModifiers & ModDeclaration) != 0);

    // Token 3: 'int' -> Type, DefaultLibrary
    CHECK(decoded[3].line == 0);
    CHECK(decoded[3].character == 21);
    CHECK(decoded[3].length == 3);
    CHECK(decoded[3].tokenType == SemType);
    CHECK((decoded[3].tokenModifiers & ModDefaultLibrary) != 0);

    // In line 1: 'return factor * 2 + offset;'
    // Find 'factor' reference in line 1:
    bool foundFactorRef = false;
    for (const auto& tok : decoded) {
        if (tok.line == 1 && tok.character == 11 && tok.length == 6) {
            foundFactorRef = true;
            CHECK(tok.tokenType == SemParameter);
            CHECK((tok.tokenModifiers & ModDeclaration) == 0); // Not declaration
        }
    }
    CHECK(foundFactorRef);
}

TEST_CASE("Analyzer semantic tokens for classes, fields, and methods", "[lsp][semantic_tokens]") {
    Analyzer analyzer;

    std::string code =
        "class Vector {\n"
        "    let x: int;\n"
        "    fn length(): int {\n"
        "        return this.x;\n"
        "    }\n"
        "}\n"
        "let v = Vector();\n"
        "v.length();\n";

    Document doc("file:///class_tokens.keet", "keet", 1, code);
    auto semTokens = analyzer.getSemanticTokens(doc);
    auto decoded = decodeSemanticTokens(semTokens.data);

    // Verify 'Vector' declaration on line 0
    bool foundVectorDecl = false;
    for (const auto& tok : decoded) {
        if (tok.line == 0 && tok.character == 6 && tok.length == 6) {
            foundVectorDecl = true;
            CHECK(tok.tokenType == SemClass);
            CHECK((tok.tokenModifiers & ModDeclaration) != 0);
        }
    }
    CHECK(foundVectorDecl);

    // Verify field 'x' declaration on line 1
    bool foundFieldDecl = false;
    for (const auto& tok : decoded) {
        if (tok.line == 1 && tok.character == 8 && tok.length == 1) {
            foundFieldDecl = true;
            CHECK(tok.tokenType == SemProperty);
            CHECK((tok.tokenModifiers & ModDeclaration) != 0);
        }
    }
    CHECK(foundFieldDecl);

    // Verify method 'length' declaration on line 2
    bool foundMethodDecl = false;
    for (const auto& tok : decoded) {
        if (tok.line == 2 && tok.character == 7 && tok.length == 6) {
            foundMethodDecl = true;
            CHECK(tok.tokenType == SemMethod);
            CHECK((tok.tokenModifiers & ModDeclaration) != 0);
        }
    }
    CHECK(foundMethodDecl);

    // Verify 'this.x' property access on line 3
    bool foundThisX = false;
    for (const auto& tok : decoded) {
        if (tok.line == 3 && tok.character == 20 && tok.length == 1) {
            foundThisX = true;
            CHECK(tok.tokenType == SemProperty);
            CHECK((tok.tokenModifiers & ModDeclaration) == 0);
        }
    }
    CHECK(foundThisX);

    // Verify 'Vector()' constructor call on line 6
    bool foundVectorCall = false;
    for (const auto& tok : decoded) {
        if (tok.line == 6 && tok.character == 8 && tok.length == 6) {
            foundVectorCall = true;
            CHECK(tok.tokenType == SemClass);
        }
    }
    CHECK(foundVectorCall);

    // Verify 'v.length()' method call on line 7
    bool foundMethodCall = false;
    for (const auto& tok : decoded) {
        if (tok.line == 7 && tok.character == 2 && tok.length == 6) {
            foundMethodCall = true;
            CHECK(tok.tokenType == SemMethod);
        }
    }
    CHECK(foundMethodCall);
}

TEST_CASE("Analyzer semantic tokens for enums and enum members", "[lsp][semantic_tokens]") {
    Analyzer analyzer;

    std::string code =
        "enum Status {\n"
        "    Pending,\n"
        "    Active,\n"
        "    Done\n"
        "}\n"
        "let s = Status.Active;\n";

    Document doc("file:///enum_tokens.keet", "keet", 1, code);
    auto semTokens = analyzer.getSemanticTokens(doc);
    auto decoded = decodeSemanticTokens(semTokens.data);

    // Verify 'Status' declaration on line 0
    bool foundEnumDecl = false;
    for (const auto& tok : decoded) {
        if (tok.line == 0 && tok.character == 5 && tok.length == 6) {
            foundEnumDecl = true;
            CHECK(tok.tokenType == SemEnum);
            CHECK((tok.tokenModifiers & ModDeclaration) != 0);
        }
    }
    CHECK(foundEnumDecl);

    // Verify 'Pending' member on line 1
    bool foundPending = false;
    for (const auto& tok : decoded) {
        if (tok.line == 1 && tok.character == 4 && tok.length == 7) {
            foundPending = true;
            CHECK(tok.tokenType == SemEnumMember);
            CHECK((tok.tokenModifiers & ModDeclaration) != 0);
        }
    }
    CHECK(foundPending);

    // Verify 'Status.Active' access on line 5: 'Status' (Enum) and 'Active' (EnumMember)
    bool foundStatusRef = false;
    bool foundActiveRef = false;
    for (const auto& tok : decoded) {
        if (tok.line == 5 && tok.character == 8 && tok.length == 6) {
            foundStatusRef = true;
            CHECK(tok.tokenType == SemEnum);
        }
        if (tok.line == 5 && tok.character == 15 && tok.length == 6) {
            foundActiveRef = true;
            CHECK(tok.tokenType == SemEnumMember);
        }
    }
    CHECK(foundStatusRef);
    CHECK(foundActiveRef);
}

TEST_CASE("Analyzer semantic tokens for constants, numbers, strings, and comments", "[lsp][semantic_tokens]") {
    Analyzer analyzer;

    std::string code =
        "// Global constant\n"
        "const MAX_USERS = 100;\n"
        "/* Block comment */\n"
        "let message = \"Hello, World!\";\n";

    Document doc("file:///const_tokens.keet", "keet", 1, code);
    auto semTokens = analyzer.getSemanticTokens(doc);
    auto decoded = decodeSemanticTokens(semTokens.data);

    // Line 0: '// Global constant' -> Comment
    CHECK(decoded[0].line == 0);
    CHECK(decoded[0].character == 0);
    CHECK(decoded[0].tokenType == SemComment);

    // Line 1: 'const' -> Keyword
    CHECK(decoded[1].line == 1);
    CHECK(decoded[1].character == 0);
    CHECK(decoded[1].tokenType == SemKeyword);

    // Line 1: 'MAX_USERS' -> Variable, Declaration | Readonly
    CHECK(decoded[2].line == 1);
    CHECK(decoded[2].character == 6);
    CHECK(decoded[2].length == 9);
    CHECK(decoded[2].tokenType == SemVariable);
    CHECK((decoded[2].tokenModifiers & ModDeclaration) != 0);
    CHECK((decoded[2].tokenModifiers & ModReadonly) != 0);

    // Line 1: '=' -> Operator
    CHECK(decoded[3].line == 1);
    CHECK(decoded[3].tokenType == SemOperator);

    // Line 1: '100' -> Number
    CHECK(decoded[4].line == 1);
    CHECK(decoded[4].character == 18);
    CHECK(decoded[4].length == 3);
    CHECK(decoded[4].tokenType == SemNumber);

    // Line 2: '/* Block comment */' -> Comment
    CHECK(decoded[5].line == 2);
    CHECK(decoded[5].character == 0);
    CHECK(decoded[5].tokenType == SemComment);

    // Line 3: '"Hello, World!"' -> String
    bool foundString = false;
    for (const auto& tok : decoded) {
        if (tok.line == 3 && tok.tokenType == SemString) {
            foundString = true;
            CHECK(tok.length == 15);
        }
    }
    CHECK(foundString);
}

TEST_CASE("Analyzer semantic tokens for standard library modules", "[lsp][semantic_tokens]") {
    Analyzer analyzer;

    std::string code =
        "import fs;\n"
        "let content = fs.readFile(\"data.txt\");\n";

    Document doc("file:///stdlib_tokens.keet", "keet", 1, code);
    auto semTokens = analyzer.getSemanticTokens(doc);
    auto decoded = decodeSemanticTokens(semTokens.data);

    // Line 0: 'fs' -> Variable with DefaultLibrary modifier
    bool foundImportFs = false;
    for (const auto& tok : decoded) {
        if (tok.line == 0 && tok.character == 7 && tok.length == 2) {
            foundImportFs = true;
            CHECK(tok.tokenType == SemVariable);
            CHECK((tok.tokenModifiers & ModDefaultLibrary) != 0);
        }
    }
    CHECK(foundImportFs);

    // Line 1: 'fs.readFile' -> 'fs' with DefaultLibrary, 'readFile' with Method and DefaultLibrary
    bool foundFsRef = false;
    bool foundReadFile = false;
    for (const auto& tok : decoded) {
        if (tok.line == 1 && tok.character == 14 && tok.length == 2) {
            foundFsRef = true;
            CHECK(tok.tokenType == SemVariable);
            CHECK((tok.tokenModifiers & ModDefaultLibrary) != 0);
        }
        if (tok.line == 1 && tok.character == 17 && tok.length == 8) {
            foundReadFile = true;
            CHECK(tok.tokenType == SemMethod);
            CHECK((tok.tokenModifiers & ModDefaultLibrary) != 0);
        }
    }
    CHECK(foundFsRef);
    CHECK(foundReadFile);
}

TEST_CASE("LSP Server textDocument/semanticTokens/full protocol integration", "[lsp][semantic_tokens]") {
    std::string in = "";
    std::ostringstream out;
    std::istringstream inStream(in);
    Transport transport(inStream, out);
    Server server(transport);

    // 1. Initialize server and verify legend capability
    std::string initReq =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
    auto initResp = server.handleRawMessage(initReq);
    REQUIRE(initResp.has_value());

    keet::json::detail::Parser pInit(*initResp);
    auto jInit = pInit.parse_document();
    const auto& initObj = std::get<JsonValue::Object>(jInit.value);
    const auto& resObj = std::get<JsonValue::Object>(initObj.at("result").value);
    const auto& capsObj = std::get<JsonValue::Object>(resObj.at("capabilities").value);
    REQUIRE(capsObj.find("semanticTokensProvider") != capsObj.end());

    const auto& semProv = std::get<JsonValue::Object>(capsObj.at("semanticTokensProvider").value);
    CHECK(std::get<bool>(semProv.at("full").value) == true);

    const auto& legendObj = std::get<JsonValue::Object>(semProv.at("legend").value);
    const auto& typesArr = std::get<JsonValue::Array>(legendObj.at("tokenTypes").value);
    const auto& modsArr = std::get<JsonValue::Array>(legendObj.at("tokenModifiers").value);
    REQUIRE(typesArr.size() == 15);
    REQUIRE(modsArr.size() == 4);

    // 2. Open document
    std::string docContent =
        "fn add(x: int): int {\\n"
        "    return x + 1;\\n"
        "}\\n";

    std::string didOpen =
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test_sem.keet\",\"languageId\":\"keet\",\"version\":1,"
        "\"text\":\"" + docContent + "\"}}}";
    server.handleRawMessage(didOpen);

    // 3. Request full semantic tokens
    std::string semReq =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/semanticTokens/full\",\"params\":{"
        "\"textDocument\":{\"uri\":\"file:///test_sem.keet\"}}}";
    auto semResp = server.handleRawMessage(semReq);
    REQUIRE(semResp.has_value());

    keet::json::detail::Parser pSem(*semResp);
    auto jSem = pSem.parse_document();
    const auto& semObj = std::get<JsonValue::Object>(jSem.value);
    REQUIRE(std::get<int64_t>(semObj.at("id").value) == 2);

    const auto& sResult = std::get<JsonValue::Object>(semObj.at("result").value);
    const auto& dataArr = std::get<JsonValue::Array>(sResult.at("data").value);
    REQUIRE_FALSE(dataArr.empty());
    REQUIRE(dataArr.size() % 5 == 0);
}
