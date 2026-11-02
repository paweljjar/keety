#include <catch2/catch_test_macros.hpp>
#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#include <sstream>

using namespace keety::lsp;

TEST_CASE("Analyzer produces live diagnostics from in-memory buffers", "[lsp][analyzer]") {
    Analyzer analyzer;

    SECTION("Valid KeetScript program yields 0 diagnostics") {
        std::string code =
            "import io;\n"
            "fn main(): void {\n"
            "    io.println(\"Hello Keet\");\n"
            "}\n";

        Document doc("file:///main.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);
        REQUIRE(diags.empty());
    }

    SECTION("Syntax error emits parse error diagnostic with accurate line and column") {
        std::string code =
            "fn broken(: void {}\n"; // missing parameter name or paren error

        Document doc("file:///syntax_err.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);

        REQUIRE_FALSE(diags.empty());
        REQUIRE(diags[0].severity == DiagnosticSeverity::Error);
        REQUIRE(diags[0].range.start.line == 0);
    }

    SECTION("Missing explicit type annotation emits E3001 diagnostic") {
        std::string code =
            "fn bad(x): void {}\n"; // parameter x missing type annotation

        Document doc("file:///missing_type.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);

        REQUIRE_FALSE(diags.empty());
        bool hasE3001 = false;
        for (const auto& d : diags) {
            if (d.code == "E3001") {
                hasE3001 = true;
                REQUIRE(d.range.start.line == 0);
                REQUIRE(d.severity == DiagnosticSeverity::Error);
                break;
            }
        }
        REQUIRE(hasE3001);
    }

    SECTION("Type mismatch emits E3003 diagnostic") {
        std::string code =
            "fn test(): int {\n"
            "    return \"string_not_int\";\n"
            "}\n";

        Document doc("file:///mismatch.keet", "keet", 1, code);
        auto diags = analyzer.analyze(doc);

        REQUIRE_FALSE(diags.empty());
        bool hasE3003 = false;
        for (const auto& d : diags) {
            if (d.code == "E3003") {
                hasE3003 = true;
                REQUIRE(d.range.start.line == 1);
                REQUIRE(d.severity == DiagnosticSeverity::Error);
                break;
            }
        }
        REQUIRE(hasE3003);
    }
}

TEST_CASE("Server publishes live diagnostics on didOpen, didChange, and didClose", "[lsp][server][diagnostics]") {
    std::istringstream in("");
    std::ostringstream out;
    Transport transport(in, out);
    Server server(std::move(transport));

    // Initialize server
    (void)server.handleRawMessage("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");

    SECTION("Publish diagnostics on didOpen with errors") {
        std::string openMsg =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
            "  \"textDocument\": {"
            "    \"uri\": \"file:///test_diag.keet\","
            "    \"languageId\": \"keet\","
            "    \"version\": 1,"
            "    \"text\": \"fn bad(): int { return \\\"hello\\\"; }\""
            "  }"
            "}}";

        (void)server.handleRawMessage(openMsg);

        std::string published = out.str();
        REQUIRE(published.find("textDocument/publishDiagnostics") != std::string::npos);
        REQUIRE(published.find("E3003") != std::string::npos);
        REQUIRE(published.find("file:///test_diag.keet") != std::string::npos);
    }

    SECTION("Clear diagnostics on didChange to valid code") {
        // First open with error
        std::string openMsg =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{"
            "  \"textDocument\": {"
            "    \"uri\": \"file:///live.keet\","
            "    \"languageId\": \"keet\","
            "    \"version\": 1,"
            "    \"text\": \"fn test(): int { return \\\"err\\\"; }\""
            "  }"
            "}}";
        (void)server.handleRawMessage(openMsg);
        out.str("");

        // Now change to valid code
        std::string changeMsg =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///live.keet\", \"version\": 2},"
            "  \"contentChanges\": [{\"text\": \"fn test(): int { return 42; }\"}]"
            "}}";
        (void)server.handleRawMessage(changeMsg);

        std::string published = out.str();
        REQUIRE(published.find("textDocument/publishDiagnostics") != std::string::npos);
        REQUIRE(published.find("\"diagnostics\":[]") != std::string::npos);
    }

    SECTION("Clear diagnostics on didClose") {
        out.str("");
        std::string closeMsg =
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{"
            "  \"textDocument\": {\"uri\": \"file:///live.keet\"}"
            "}}";
        (void)server.handleRawMessage(closeMsg);

        std::string published = out.str();
        REQUIRE(published.find("textDocument/publishDiagnostics") != std::string::npos);
        REQUIRE(published.find("\"diagnostics\":[]") != std::string::npos);
    }
}
