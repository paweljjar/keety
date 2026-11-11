#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <keet/json.hpp>

namespace keety::lsp {

/// Type-safe JSON DOM node matching keet::json::detail::JsonNode
using JsonValue = keet::json::detail::JsonNode;

inline JsonValue makeJsonObject(std::initializer_list<std::pair<std::string, JsonValue>> init) {
    JsonValue::Object obj;
    for (const auto& [k, v] : init) {
        obj.insert_or_assign(k, v);
    }
    return JsonValue(std::move(obj));
}

inline JsonValue makeJsonArray(std::initializer_list<JsonValue> init) {
    JsonValue::Array arr(init);
    return JsonValue(std::move(arr));
}

// JSON-RPC Error Codes
enum class JsonRpcErrorCode : int {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    RequestCancelled = -32800,
};

// Represents a JSON-RPC request / notification / response ID (integer or string)
using RequestId = std::variant<int64_t, std::string>;

std::string requestIdToString(const RequestId& id);

struct Position {
    uint32_t line = 0;       // 0-indexed
    uint32_t character = 0;  // 0-indexed

    bool operator==(const Position& other) const noexcept {
        return line == other.line && character == other.character;
    }

    JsonValue toJson() const;
    static Position fromJson(const JsonValue& j);
};

struct Range {
    Position start;
    Position end;

    bool operator==(const Range& other) const noexcept {
        return start == other.start && end == other.end;
    }

    JsonValue toJson() const;
    static Range fromJson(const JsonValue& j);
};

struct TextDocumentItem {
    std::string uri;
    std::string languageId;
    int32_t version = 0;
    std::string text;

    static TextDocumentItem fromJson(const JsonValue& j);
};

struct TextDocumentIdentifier {
    std::string uri;

    static TextDocumentIdentifier fromJson(const JsonValue& j);
};

struct VersionedTextDocumentIdentifier {
    std::string uri;
    std::optional<int32_t> version;

    static VersionedTextDocumentIdentifier fromJson(const JsonValue& j);
};

struct TextDocumentContentChangeEvent {
    std::optional<Range> range; // null if full sync
    std::string text;

    static TextDocumentContentChangeEvent fromJson(const JsonValue& j);
};

enum class DiagnosticSeverity : int {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

struct DiagnosticRelatedInformation {
    std::string uri;
    Range range;
    std::string message;

    JsonValue toJson() const;
};

struct Diagnostic {
    Range range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;
    std::string source = "keety";
    std::string message;
    std::vector<DiagnosticRelatedInformation> relatedInformation;

    JsonValue toJson() const;
};

struct Location {
    std::string uri;
    Range range;

    JsonValue toJson() const;
    static Location fromJson(const JsonValue& j);
};

struct MarkupContent {
    std::string kind = "markdown";
    std::string value;

    JsonValue toJson() const;
    static MarkupContent fromJson(const JsonValue& j);
};

struct Hover {
    MarkupContent contents;
    std::optional<Range> range;

    JsonValue toJson() const;
    static Hover fromJson(const JsonValue& j);
};

enum class CompletionItemKind : int {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum class InsertTextFormat : int {
    PlainText = 1,
    Snippet = 2,
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind = CompletionItemKind::Text;
    std::string detail;
    std::string documentation;
    std::string insertText;
    InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;

    JsonValue toJson() const;
};

struct CompletionList {
    bool isIncomplete = false;
    std::vector<CompletionItem> items;

    JsonValue toJson() const;
};

enum class SymbolKind : int {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

struct DocumentSymbol {
    std::string name;
    std::string detail;
    SymbolKind kind = SymbolKind::Variable;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;

    JsonValue toJson() const;
    static DocumentSymbol fromJson(const JsonValue& j);
};

struct DocumentSymbolParams {
    TextDocumentIdentifier textDocument;

    static DocumentSymbolParams fromJson(const JsonValue& j);
};

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;

    static TextDocumentPositionParams fromJson(const JsonValue& j);
};

struct ReferenceContext {
    bool includeDeclaration = true;

    JsonValue toJson() const;
    static ReferenceContext fromJson(const JsonValue& j);
};

struct ReferenceParams {
    TextDocumentIdentifier textDocument;
    Position position;
    ReferenceContext context;

    JsonValue toJson() const;
    static ReferenceParams fromJson(const JsonValue& j);
};

struct PrepareRenameParams {
    TextDocumentIdentifier textDocument;
    Position position;

    JsonValue toJson() const;
    static PrepareRenameParams fromJson(const JsonValue& j);
};

struct PrepareRenameResult {
    Range range;
    std::string placeholder;

    JsonValue toJson() const;
    static PrepareRenameResult fromJson(const JsonValue& j);
};

struct RenameParams {
    TextDocumentIdentifier textDocument;
    Position position;
    std::string newName;

    JsonValue toJson() const;
    static RenameParams fromJson(const JsonValue& j);
};

struct ParameterInformation {
    std::string label;
    std::optional<MarkupContent> documentation;

    JsonValue toJson() const;
    static ParameterInformation fromJson(const JsonValue& j);
};

struct SignatureInformation {
    std::string label;
    std::optional<MarkupContent> documentation;
    std::vector<ParameterInformation> parameters;
    std::optional<uint32_t> activeParameter;

    JsonValue toJson() const;
    static SignatureInformation fromJson(const JsonValue& j);
};

struct SignatureHelp {
    std::vector<SignatureInformation> signatures;
    uint32_t activeSignature = 0;
    uint32_t activeParameter = 0;

    JsonValue toJson() const;
    static SignatureHelp fromJson(const JsonValue& j);
};

struct SignatureHelpParams {
    TextDocumentIdentifier textDocument;
    Position position;

    JsonValue toJson() const;
    static SignatureHelpParams fromJson(const JsonValue& j);
};

struct SemanticTokensLegend {
    std::vector<std::string> tokenTypes;
    std::vector<std::string> tokenModifiers;

    JsonValue toJson() const;
    static SemanticTokensLegend fromJson(const JsonValue& j);
    static SemanticTokensLegend defaultLegend();
};

struct SemanticTokensParams {
    TextDocumentIdentifier textDocument;

    JsonValue toJson() const;
    static SemanticTokensParams fromJson(const JsonValue& j);
};

struct SemanticTokens {
    std::optional<std::string> resultId;
    std::vector<uint32_t> data;

    JsonValue toJson() const;
    static SemanticTokens fromJson(const JsonValue& j);
};

struct PublishDiagnosticsParams {
    std::string uri;
    std::optional<int32_t> version;
    std::vector<Diagnostic> diagnostics;

    JsonValue toJson() const;
};

struct FormattingOptions {
    uint32_t tabSize = 4;
    bool insertSpaces = true;
    bool trimTrailingWhitespace = false;
    bool insertFinalNewline = false;
    bool trimFinalNewlines = false;

    static FormattingOptions fromJson(const JsonValue& j);
};

struct DocumentFormattingParams {
    TextDocumentIdentifier textDocument;
    FormattingOptions options;

    static DocumentFormattingParams fromJson(const JsonValue& j);
};

struct TextEdit {
    Range range;
    std::string newText;

    JsonValue toJson() const;
    static TextEdit fromJson(const JsonValue& j);
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<TextEdit>> changes;

    JsonValue toJson() const;
    static WorkspaceEdit fromJson(const JsonValue& j);
};

struct CodeActionContext {
    std::vector<Diagnostic> diagnostics;

    static CodeActionContext fromJson(const JsonValue& j);
};

struct CodeActionParams {
    TextDocumentIdentifier textDocument;
    Range range;
    CodeActionContext context;

    static CodeActionParams fromJson(const JsonValue& j);
};

struct CodeAction {
    std::string title;
    std::string kind = "quickfix";
    std::vector<Diagnostic> diagnostics;
    bool isPreferred = true;
    std::optional<WorkspaceEdit> edit;

    JsonValue toJson() const;
};

// Outgoing JSON-RPC message
struct JsonRpcResponse {
    std::optional<RequestId> id;
    std::optional<JsonValue> result;
    std::optional<JsonValue> error;

    std::string serialize() const;

    static JsonRpcResponse makeResult(RequestId id, JsonValue result);
    static JsonRpcResponse makeError(std::optional<RequestId> id, JsonRpcErrorCode code, const std::string& message, std::optional<JsonValue> data = std::nullopt);
};

struct JsonRpcNotification {
    std::string method;
    JsonValue params;

    std::string serialize() const;
};

} // namespace keety::lsp
