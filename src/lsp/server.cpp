#include <keety/lsp/server.hpp>

#include <iostream>

namespace keety::lsp {

namespace {

const JsonValue::Object* asObject(const JsonValue& j) {
    if (j.is_object()) {
        return &std::get<JsonValue::Object>(j.value);
    }
    return nullptr;
}

const JsonValue* findField(const JsonValue::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    return (it != obj.end()) ? &it->second : nullptr;
}

std::string getStringField(const JsonValue::Object& obj, const std::string& key, const std::string& def = "") {
    if (auto* v = findField(obj, key); v && v->is_string()) {
        return std::get<std::string>(v->value);
    }
    return def;
}

} // namespace

Server::Server(Transport transport)
    : transport_(std::move(transport)) {}

int Server::run() {
    while (!shouldExit_) {
        auto msgOpt = transport_.readMessage();
        if (!msgOpt.has_value()) {
            break; // EOF
        }

        try {
            keet::json::detail::Parser parser(*msgOpt);
            JsonValue j = parser.parse_document();
            dispatchMessage(j);
        } catch (const std::exception& e) {
            auto errResp = JsonRpcResponse::makeError(
                std::nullopt,
                JsonRpcErrorCode::ParseError,
                std::string("failed to parse JSON-RPC payload: ") + e.what()
            );
            transport_.writeMessage(errResp.serialize());
        }
    }
    return exitCode_;
}

std::optional<std::string> Server::handleRawMessage(const std::string& rawJson) {
    try {
        keet::json::detail::Parser parser(rawJson);
        JsonValue j = parser.parse_document();

        auto* obj = asObject(j);
        if (!obj) {
            return JsonRpcResponse::makeError(std::nullopt, JsonRpcErrorCode::InvalidRequest, "expected json object").serialize();
        }

        auto* idNode = findField(*obj, "id");
        if (idNode && !idNode->is_null()) {
            RequestId id;
            if (idNode->is_int()) {
                id = std::get<int64_t>(idNode->value);
            } else if (idNode->is_string()) {
                id = std::get<std::string>(idNode->value);
            } else {
                id = int64_t(0);
            }

            std::string method = getStringField(*obj, "method");
            auto* paramsNode = findField(*obj, "params");
            JsonValue params = paramsNode ? *paramsNode : JsonValue(JsonValue::Object{});

            if (method == "initialize") {
                auto res = handleInitialize(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "textDocument/definition") {
                auto res = handleDefinition(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "textDocument/hover") {
                auto res = handleHover(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "textDocument/completion") {
                auto res = handleCompletion(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "textDocument/documentSymbol") {
                auto res = handleDocumentSymbol(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "textDocument/formatting") {
                auto res = handleFormatting(params);
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else if (method == "shutdown") {
                auto res = handleShutdown();
                return JsonRpcResponse::makeResult(id, res).serialize();
            } else {
                return JsonRpcResponse::makeError(id, JsonRpcErrorCode::MethodNotFound, "method not found: " + method).serialize();
            }
        } else {
            std::string method = getStringField(*obj, "method");
            auto* paramsNode = findField(*obj, "params");
            JsonValue params = paramsNode ? *paramsNode : JsonValue(JsonValue::Object{});
            handleNotification(method, params);
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        return JsonRpcResponse::makeError(
            std::nullopt,
            JsonRpcErrorCode::ParseError,
            std::string("parse error: ") + e.what()
        ).serialize();
    }
}

void Server::dispatchMessage(const JsonValue& j) {
    auto* obj = asObject(j);
    if (!obj) {
        auto resp = JsonRpcResponse::makeError(std::nullopt, JsonRpcErrorCode::InvalidRequest, "expected json object");
        transport_.writeMessage(resp.serialize());
        return;
    }

    auto* rpc = findField(*obj, "jsonrpc");
    if (!rpc || !rpc->is_string() || std::get<std::string>(rpc->value) != "2.0") {
        auto resp = JsonRpcResponse::makeError(std::nullopt, JsonRpcErrorCode::InvalidRequest, "missing jsonrpc 2.0 version");
        transport_.writeMessage(resp.serialize());
        return;
    }

    auto* idNode = findField(*obj, "id");
    bool hasId = idNode && !idNode->is_null();
    std::string method = getStringField(*obj, "method");
    auto* paramsNode = findField(*obj, "params");
    JsonValue params = paramsNode ? *paramsNode : JsonValue(JsonValue::Object{});

    if (hasId) {
        RequestId id;
        if (idNode->is_int()) {
            id = std::get<int64_t>(idNode->value);
        } else if (idNode->is_string()) {
            id = std::get<std::string>(idNode->value);
        } else {
            id = int64_t(0);
        }
        handleRequest(id, method, params);
    } else {
        handleNotification(method, params);
    }
}

void Server::handleRequest(const RequestId& id, const std::string& method, const JsonValue& params) {
    if (method == "initialize") {
        auto res = handleInitialize(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (!initialized_) {
        auto resp = JsonRpcResponse::makeError(
            id,
            JsonRpcErrorCode::ServerNotInitialized,
            "server not initialized; call 'initialize' first"
        );
        transport_.writeMessage(resp.serialize());
        return;
    }

    if (method == "shutdown") {
        auto res = handleShutdown();
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (method == "textDocument/definition") {
        auto res = handleDefinition(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (method == "textDocument/hover") {
        auto res = handleHover(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (method == "textDocument/completion") {
        auto res = handleCompletion(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (method == "textDocument/documentSymbol") {
        auto res = handleDocumentSymbol(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    if (method == "textDocument/formatting") {
        auto res = handleFormatting(params);
        transport_.writeMessage(JsonRpcResponse::makeResult(id, res).serialize());
        return;
    }

    auto resp = JsonRpcResponse::makeError(id, JsonRpcErrorCode::MethodNotFound, "method not found: " + method);
    transport_.writeMessage(resp.serialize());
}

void Server::handleNotification(const std::string& method, const JsonValue& params) {
    if (method == "initialized") {
        handleInitialized(params);
        return;
    }

    if (method == "exit") {
        handleExit();
        return;
    }

    if (!initialized_) {
        // Discard uninitialized notifications
        return;
    }

    if (method == "textDocument/didOpen") {
        handleDidOpen(params);
    } else if (method == "textDocument/didChange") {
        handleDidChange(params);
    } else if (method == "textDocument/didClose") {
        handleDidClose(params);
    }
}

JsonValue Server::handleInitialize(const JsonValue& /*params*/) {
    initialized_ = true;

    // Report server capabilities
    JsonValue::Object caps;

    JsonValue::Object sync;
    sync["openClose"] = JsonValue(true);
    sync["change"] = JsonValue(int64_t(1)); // 1 = Full sync
    caps["textDocumentSync"] = JsonValue(std::move(sync));

    caps["definitionProvider"] = JsonValue(true);
    caps["hoverProvider"] = JsonValue(true);

    JsonValue::Object compProvider;
    compProvider["resolveProvider"] = JsonValue(false);
    JsonValue::Array trigChars;
    trigChars.push_back(JsonValue(std::string(".")));
    trigChars.push_back(JsonValue(std::string("\"")));
    trigChars.push_back(JsonValue(std::string("/")));
    compProvider["triggerCharacters"] = JsonValue(std::move(trigChars));
    caps["completionProvider"] = JsonValue(std::move(compProvider));

    caps["documentSymbolProvider"] = JsonValue(true);
    caps["documentFormattingProvider"] = JsonValue(true);

    JsonValue::Object serverInfo;
    serverInfo["name"] = JsonValue(std::string("keet-lsp"));
    serverInfo["version"] = JsonValue(std::string("0.1.0"));

    JsonValue::Object result;
    result["capabilities"] = JsonValue(std::move(caps));
    result["serverInfo"] = JsonValue(std::move(serverInfo));

    return JsonValue(std::move(result));
}

void Server::handleInitialized(const JsonValue& /*params*/) {
    // Client acknowledged initialization
}

JsonValue Server::handleShutdown() {
    shutdownReceived_ = true;
    return JsonValue(std::monostate{});
}

void Server::handleExit() {
    shouldExit_ = true;
    exitCode_ = shutdownReceived_ ? 0 : 1;
}

void Server::handleDidOpen(const JsonValue& params) {
    if (auto* obj = asObject(params)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            auto item = TextDocumentItem::fromJson(*docNode);
            docManager_.openDocument(item);
            publishDiagnostics(item.uri, item.version);
        }
    }
}

void Server::handleDidChange(const JsonValue& params) {
    if (auto* obj = asObject(params)) {
        auto* docNode = findField(*obj, "textDocument");
        auto* changesNode = findField(*obj, "contentChanges");
        if (docNode && changesNode && changesNode->is_array()) {
            auto docId = VersionedTextDocumentIdentifier::fromJson(*docNode);
            int32_t version = docId.version.value_or(0);

            const auto& arr = std::get<JsonValue::Array>(changesNode->value);
            if (!arr.empty()) {
                // Under Full Sync (1), the last element contains the full updated text
                auto lastChange = TextDocumentContentChangeEvent::fromJson(arr.back());
                docManager_.updateDocument(docId.uri, version, lastChange.text);
                publishDiagnostics(docId.uri, version);
            }
        }
    }
}

void Server::handleDidClose(const JsonValue& params) {
    if (auto* obj = asObject(params)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            auto docId = TextDocumentIdentifier::fromJson(*docNode);
            docManager_.closeDocument(docId.uri);

            // Clear diagnostics for closed document
            PublishDiagnosticsParams clearParams;
            clearParams.uri = docId.uri;
            JsonRpcNotification notice;
            notice.method = "textDocument/publishDiagnostics";
            notice.params = clearParams.toJson();
            transport_.writeMessage(notice.serialize());
        }
    }
}

JsonValue Server::handleDefinition(const JsonValue& params) {
    auto p = TextDocumentPositionParams::fromJson(params);
    auto docOpt = docManager_.getDocument(p.textDocument.uri);
    if (!docOpt.has_value()) {
        return JsonValue(std::monostate{});
    }
    auto locOpt = analyzer_.findDefinition(*docOpt, p.position, &docManager_);
    if (!locOpt.has_value()) {
        return JsonValue(std::monostate{});
    }
    return locOpt->toJson();
}

JsonValue Server::handleHover(const JsonValue& params) {
    auto p = TextDocumentPositionParams::fromJson(params);
    auto docOpt = docManager_.getDocument(p.textDocument.uri);
    if (!docOpt.has_value()) {
        return JsonValue(std::monostate{});
    }
    auto hoverOpt = analyzer_.getHover(*docOpt, p.position, &docManager_);
    if (!hoverOpt.has_value()) {
        return JsonValue(std::monostate{});
    }
    return hoverOpt->toJson();
}

JsonValue Server::handleCompletion(const JsonValue& params) {
    auto p = TextDocumentPositionParams::fromJson(params);
    auto docOpt = docManager_.getDocument(p.textDocument.uri);
    if (!docOpt.has_value()) {
        CompletionList emptyList;
        return emptyList.toJson();
    }
    auto list = analyzer_.getCompletions(*docOpt, p.position, &docManager_);
    return list.toJson();
}

JsonValue Server::handleDocumentSymbol(const JsonValue& params) {
    auto p = DocumentSymbolParams::fromJson(params);
    auto docOpt = docManager_.getDocument(p.textDocument.uri);
    if (!docOpt.has_value()) {
        JsonValue::Array emptyArr;
        return JsonValue(std::move(emptyArr));
    }
    auto symbols = analyzer_.getDocumentSymbols(*docOpt);
    JsonValue::Array arr;
    for (const auto& sym : symbols) {
        arr.push_back(sym.toJson());
    }
    return JsonValue(std::move(arr));
}

JsonValue Server::handleFormatting(const JsonValue& params) {
    auto p = DocumentFormattingParams::fromJson(params);
    auto docOpt = docManager_.getDocument(p.textDocument.uri);
    if (!docOpt.has_value()) {
        JsonValue::Array emptyArr;
        return JsonValue(std::move(emptyArr));
    }
    auto edits = analyzer_.format(*docOpt, p.options);
    JsonValue::Array arr;
    for (const auto& edit : edits) {
        arr.push_back(edit.toJson());
    }
    return JsonValue(std::move(arr));
}

void Server::publishDiagnostics(const std::string& uri, int32_t version) {
    auto docOpt = docManager_.getDocument(uri);
    if (!docOpt.has_value()) {
        return;
    }

    auto diags = analyzer_.analyze(*docOpt);

    PublishDiagnosticsParams publishParams;
    publishParams.uri = uri;
    publishParams.version = version;
    publishParams.diagnostics = std::move(diags);

    JsonRpcNotification notice;
    notice.method = "textDocument/publishDiagnostics";
    notice.params = publishParams.toJson();
    transport_.writeMessage(notice.serialize());
}

} // namespace keety::lsp
