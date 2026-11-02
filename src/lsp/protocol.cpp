#include <keety/lsp/protocol.hpp>

#include <stdexcept>

namespace keety::lsp {

namespace {

const JsonValue::Object* asObject(const JsonValue& j) {
    if (j.is_object()) {
        return &std::get<JsonValue::Object>(j.value);
    }
    return nullptr;
}

const std::string* asString(const JsonValue& j) {
    if (j.is_string()) {
        return &std::get<std::string>(j.value);
    }
    return nullptr;
}

const int64_t* asInt(const JsonValue& j) {
    if (j.is_int()) {
        return &std::get<int64_t>(j.value);
    }
    return nullptr;
}

const JsonValue::Array* asArray(const JsonValue& j) {
    if (j.is_array()) {
        return &std::get<JsonValue::Array>(j.value);
    }
    return nullptr;
}

const JsonValue* findField(const JsonValue::Object& obj, const std::string& key) {
    auto it = obj.find(key);
    return (it != obj.end()) ? &it->second : nullptr;
}

} // namespace

std::string requestIdToString(const RequestId& id) {
    if (std::holds_alternative<int64_t>(id)) {
        return std::to_string(std::get<int64_t>(id));
    }
    return std::get<std::string>(id);
}

JsonValue Position::toJson() const {
    JsonValue::Object obj;
    obj["line"] = JsonValue(static_cast<int64_t>(line));
    obj["character"] = JsonValue(static_cast<int64_t>(character));
    return JsonValue(std::move(obj));
}

Position Position::fromJson(const JsonValue& j) {
    Position pos;
    if (auto* obj = asObject(j)) {
        if (auto* l = findField(*obj, "line"); l && l->is_int()) {
            pos.line = static_cast<uint32_t>(std::get<int64_t>(l->value));
        }
        if (auto* c = findField(*obj, "character"); c && c->is_int()) {
            pos.character = static_cast<uint32_t>(std::get<int64_t>(c->value));
        }
    }
    return pos;
}

JsonValue Range::toJson() const {
    JsonValue::Object obj;
    obj["start"] = start.toJson();
    obj["end"] = end.toJson();
    return JsonValue(std::move(obj));
}

Range Range::fromJson(const JsonValue& j) {
    Range r;
    if (auto* obj = asObject(j)) {
        if (auto* s = findField(*obj, "start")) r.start = Position::fromJson(*s);
        if (auto* e = findField(*obj, "end")) r.end = Position::fromJson(*e);
    }
    return r;
}

TextDocumentItem TextDocumentItem::fromJson(const JsonValue& j) {
    TextDocumentItem item;
    if (auto* obj = asObject(j)) {
        if (auto* u = findField(*obj, "uri"); u && u->is_string()) item.uri = std::get<std::string>(u->value);
        if (auto* l = findField(*obj, "languageId"); l && l->is_string()) item.languageId = std::get<std::string>(l->value);
        if (auto* v = findField(*obj, "version"); v && v->is_int()) item.version = static_cast<int32_t>(std::get<int64_t>(v->value));
        if (auto* t = findField(*obj, "text"); t && t->is_string()) item.text = std::get<std::string>(t->value);
    }
    return item;
}

TextDocumentIdentifier TextDocumentIdentifier::fromJson(const JsonValue& j) {
    TextDocumentIdentifier id;
    if (auto* obj = asObject(j)) {
        if (auto* u = findField(*obj, "uri"); u && u->is_string()) {
            id.uri = std::get<std::string>(u->value);
        }
    }
    return id;
}

VersionedTextDocumentIdentifier VersionedTextDocumentIdentifier::fromJson(const JsonValue& j) {
    VersionedTextDocumentIdentifier id;
    if (auto* obj = asObject(j)) {
        if (auto* u = findField(*obj, "uri"); u && u->is_string()) id.uri = std::get<std::string>(u->value);
        if (auto* v = findField(*obj, "version"); v && v->is_int()) id.version = static_cast<int32_t>(std::get<int64_t>(v->value));
    }
    return id;
}

TextDocumentContentChangeEvent TextDocumentContentChangeEvent::fromJson(const JsonValue& j) {
    TextDocumentContentChangeEvent ev;
    if (auto* obj = asObject(j)) {
        if (auto* r = findField(*obj, "range"); r && !r->is_null()) {
            ev.range = Range::fromJson(*r);
        }
        if (auto* t = findField(*obj, "text"); t && t->is_string()) {
            ev.text = std::get<std::string>(t->value);
        }
    }
    return ev;
}

JsonValue DiagnosticRelatedInformation::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object loc;
    loc["uri"] = JsonValue(uri);
    loc["range"] = range.toJson();
    obj["location"] = JsonValue(std::move(loc));
    obj["message"] = JsonValue(message);
    return JsonValue(std::move(obj));
}

JsonValue Diagnostic::toJson() const {
    JsonValue::Object obj;
    obj["range"] = range.toJson();
    obj["severity"] = JsonValue(static_cast<int64_t>(severity));
    if (!code.empty()) {
        obj["code"] = JsonValue(code);
    }
    obj["source"] = JsonValue(source);
    obj["message"] = JsonValue(message);

    if (!relatedInformation.empty()) {
        JsonValue::Array relArr;
        for (const auto& rel : relatedInformation) {
            relArr.push_back(rel.toJson());
        }
        obj["relatedInformation"] = JsonValue(std::move(relArr));
    }
    return JsonValue(std::move(obj));
}

JsonValue Location::toJson() const {
    JsonValue::Object obj;
    obj["uri"] = JsonValue(uri);
    obj["range"] = range.toJson();
    return JsonValue(std::move(obj));
}

Location Location::fromJson(const JsonValue& j) {
    Location loc;
    if (auto* obj = asObject(j)) {
        if (auto* u = findField(*obj, "uri"); u && u->is_string()) {
            loc.uri = std::get<std::string>(u->value);
        }
        if (auto* r = findField(*obj, "range")) {
            loc.range = Range::fromJson(*r);
        }
    }
    return loc;
}

JsonValue MarkupContent::toJson() const {
    JsonValue::Object obj;
    obj["kind"] = JsonValue(kind);
    obj["value"] = JsonValue(value);
    return JsonValue(std::move(obj));
}

MarkupContent MarkupContent::fromJson(const JsonValue& j) {
    MarkupContent mc;
    if (auto* obj = asObject(j)) {
        if (auto* k = findField(*obj, "kind"); k && k->is_string()) {
            mc.kind = std::get<std::string>(k->value);
        }
        if (auto* v = findField(*obj, "value"); v && v->is_string()) {
            mc.value = std::get<std::string>(v->value);
        }
    }
    return mc;
}

JsonValue Hover::toJson() const {
    JsonValue::Object obj;
    obj["contents"] = contents.toJson();
    if (range.has_value()) {
        obj["range"] = range->toJson();
    }
    return JsonValue(std::move(obj));
}

Hover Hover::fromJson(const JsonValue& j) {
    Hover h;
    if (auto* obj = asObject(j)) {
        if (auto* c = findField(*obj, "contents")) {
            h.contents = MarkupContent::fromJson(*c);
        }
        if (auto* r = findField(*obj, "range")) {
            h.range = Range::fromJson(*r);
        }
    }
    return h;
}

JsonValue CompletionItem::toJson() const {
    JsonValue::Object obj;
    obj["label"] = JsonValue(label);
    obj["kind"] = JsonValue(static_cast<int64_t>(kind));
    if (!detail.empty()) {
        obj["detail"] = JsonValue(detail);
    }
    if (!documentation.empty()) {
        JsonValue::Object docObj;
        docObj["kind"] = JsonValue(std::string("markdown"));
        docObj["value"] = JsonValue(documentation);
        obj["documentation"] = JsonValue(std::move(docObj));
    }
    if (!insertText.empty()) {
        obj["insertText"] = JsonValue(insertText);
    }
    if (insertTextFormat == InsertTextFormat::Snippet) {
        obj["insertTextFormat"] = JsonValue(static_cast<int64_t>(InsertTextFormat::Snippet));
    }
    return JsonValue(std::move(obj));
}

JsonValue CompletionList::toJson() const {
    JsonValue::Object obj;
    obj["isIncomplete"] = JsonValue(isIncomplete);
    JsonValue::Array arr;
    for (const auto& item : items) {
        arr.push_back(item.toJson());
    }
    obj["items"] = JsonValue(std::move(arr));
    return JsonValue(std::move(obj));
}

JsonValue DocumentSymbol::toJson() const {
    JsonValue::Object obj;
    obj["name"] = JsonValue(name);
    if (!detail.empty()) {
        obj["detail"] = JsonValue(detail);
    }
    obj["kind"] = JsonValue(static_cast<int64_t>(kind));
    obj["range"] = range.toJson();
    obj["selectionRange"] = selectionRange.toJson();
    if (!children.empty()) {
        JsonValue::Array arr;
        for (const auto& child : children) {
            arr.push_back(child.toJson());
        }
        obj["children"] = JsonValue(std::move(arr));
    }
    return JsonValue(std::move(obj));
}

DocumentSymbol DocumentSymbol::fromJson(const JsonValue& j) {
    DocumentSymbol ds;
    if (auto* obj = asObject(j)) {
        if (auto* nameNode = findField(*obj, "name")) {
            if (auto* s = asString(*nameNode)) ds.name = *s;
        }
        if (auto* detailNode = findField(*obj, "detail")) {
            if (auto* s = asString(*detailNode)) ds.detail = *s;
        }
        if (auto* kindNode = findField(*obj, "kind")) {
            if (auto* k = asInt(*kindNode)) ds.kind = static_cast<SymbolKind>(*k);
        }
        if (auto* rangeNode = findField(*obj, "range")) {
            ds.range = Range::fromJson(*rangeNode);
        }
        if (auto* selNode = findField(*obj, "selectionRange")) {
            ds.selectionRange = Range::fromJson(*selNode);
        }
        if (auto* childrenNode = findField(*obj, "children")) {
            if (auto* arr = asArray(*childrenNode)) {
                for (const auto& childNode : *arr) {
                    ds.children.push_back(DocumentSymbol::fromJson(childNode));
                }
            }
        }
    }
    return ds;
}

DocumentSymbolParams DocumentSymbolParams::fromJson(const JsonValue& j) {
    DocumentSymbolParams p;
    if (auto* obj = asObject(j)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*docNode);
        }
    }
    return p;
}

TextDocumentPositionParams TextDocumentPositionParams::fromJson(const JsonValue& j) {
    TextDocumentPositionParams p;
    if (auto* obj = asObject(j)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*docNode);
        }
        if (auto* posNode = findField(*obj, "position")) {
            p.position = Position::fromJson(*posNode);
        }
    }
    return p;
}

JsonValue PublishDiagnosticsParams::toJson() const {
    JsonValue::Object obj;
    obj["uri"] = JsonValue(uri);
    if (version.has_value()) {
        obj["version"] = JsonValue(static_cast<int64_t>(*version));
    }
    JsonValue::Array diagArr;
    for (const auto& d : diagnostics) {
        diagArr.push_back(d.toJson());
    }
    obj["diagnostics"] = JsonValue(std::move(diagArr));
    return JsonValue(std::move(obj));
}

std::string JsonRpcResponse::serialize() const {
    JsonValue::Object obj;
    obj["jsonrpc"] = JsonValue(std::string("2.0"));

    if (id.has_value()) {
        if (std::holds_alternative<int64_t>(*id)) {
            obj["id"] = JsonValue(std::get<int64_t>(*id));
        } else {
            obj["id"] = JsonValue(std::get<std::string>(*id));
        }
    } else {
        obj["id"] = JsonValue(std::monostate{});
    }

    if (error.has_value()) {
        obj["error"] = *error;
    } else if (result.has_value()) {
        obj["result"] = *result;
    } else {
        obj["result"] = JsonValue(std::monostate{});
    }

    return keet::json::detail::dump_node(JsonValue(std::move(obj)), 0);
}

JsonRpcResponse JsonRpcResponse::makeResult(RequestId id, JsonValue result) {
    JsonRpcResponse res;
    res.id = std::move(id);
    res.result = std::move(result);
    return res;
}

JsonRpcResponse JsonRpcResponse::makeError(
    std::optional<RequestId> id,
    JsonRpcErrorCode code,
    const std::string& message,
    std::optional<JsonValue> data
) {
    JsonRpcResponse res;
    res.id = std::move(id);

    JsonValue::Object err;
    err["code"] = JsonValue(static_cast<int64_t>(code));
    err["message"] = JsonValue(message);
    if (data.has_value()) {
        err["data"] = std::move(*data);
    }
    res.error = JsonValue(std::move(err));
    return res;
}

std::string JsonRpcNotification::serialize() const {
    JsonValue::Object obj;
    obj["jsonrpc"] = JsonValue(std::string("2.0"));
    obj["method"] = JsonValue(method);
    obj["params"] = params;
    return keet::json::detail::dump_node(JsonValue(std::move(obj)), 0);
}

} // namespace keety::lsp
