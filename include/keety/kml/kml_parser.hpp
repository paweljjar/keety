#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/kml/kml_ast.hpp>
#include <keety/kml/kml_lexer.hpp>
#include <memory>
#include <string_view>

namespace keety::kml {

class KmlParser {
public:
    KmlParser(std::string_view source,
              diag::FileId fileId,
              diag::SourceManager* sourceManager = nullptr,
              diag::DiagnosticEmitter* emitter = nullptr);

    /// Main entry point: parses the complete Single File Component
    KmlDocument parseDocument(std::string componentName = "Component");

    // Internal block parsers
    KmlScriptBlock parseScriptBlock(std::string_view rawScript, diag::SourceSpan span);
    std::vector<std::unique_ptr<KmlTemplateNode>> parseTemplateBlock();
    KmlStyleBlock parseStyleBlock(std::string_view rawStyle, diag::SourceSpan span);

    // Template element and control block parsers
    std::unique_ptr<KmlTemplateNode> parseTemplateNode();
    std::unique_ptr<KmlElementNode> parseElement();
    std::unique_ptr<KmlIfBlock> parseIfBlock(const KmlToken& ifTok);
    std::unique_ptr<KmlForBlock> parseForBlock(const KmlToken& forTok);
    std::unique_ptr<KmlAwaitBlock> parseAwaitBlock(const KmlToken& awaitTok);

private:
    void parseTagAttributes(KmlElementNode& element);
    void extractScriptRunesAndProps(KmlScriptBlock& scriptBlock);

    // Lookahead and token management
    const KmlToken& peek(size_t k = 0);
    const KmlToken& advance();
    bool match(KmlTokenKind kind);
    bool check(KmlTokenKind kind, size_t k = 0);

    KmlLexer lexer_;
    diag::FileId fileId_;
    [[maybe_unused]] diag::SourceManager* sourceMgr_;
    diag::DiagnosticEmitter* emitter_;
    std::vector<KmlToken> buffer_;
};

} // namespace keety::kml
