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

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;

    static TextDocumentPositionParams fromJson(const JsonValue& j);
};

struct PublishDiagnosticsParams {
    std::string uri;
    std::optional<int32_t> version;
    std::vector<Diagnostic> diagnostics;

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
