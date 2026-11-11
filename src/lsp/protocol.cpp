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

JsonValue ReferenceContext::toJson() const {
    JsonValue::Object obj;
    obj["includeDeclaration"] = JsonValue(includeDeclaration);
    return JsonValue(std::move(obj));
}

ReferenceContext ReferenceContext::fromJson(const JsonValue& j) {
    ReferenceContext ctx;
    if (auto* obj = asObject(j)) {
        if (auto* inc = findField(*obj, "includeDeclaration"); inc && inc->is_bool()) {
            ctx.includeDeclaration = std::get<bool>(inc->value);
        }
    }
    return ctx;
}

JsonValue ReferenceParams::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object docObj;
    docObj["uri"] = JsonValue(textDocument.uri);
    obj["textDocument"] = JsonValue(std::move(docObj));
    obj["position"] = position.toJson();
    obj["context"] = context.toJson();
    return JsonValue(std::move(obj));
}

ReferenceParams ReferenceParams::fromJson(const JsonValue& j) {
    ReferenceParams p;
    if (auto* obj = asObject(j)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*docNode);
        }
        if (auto* posNode = findField(*obj, "position")) {
            p.position = Position::fromJson(*posNode);
        }
        if (auto* ctxNode = findField(*obj, "context")) {
            p.context = ReferenceContext::fromJson(*ctxNode);
        }
    }
    return p;
}

JsonValue PrepareRenameParams::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object docObj;
    docObj["uri"] = JsonValue(textDocument.uri);
    obj["textDocument"] = JsonValue(std::move(docObj));
    obj["position"] = position.toJson();
    return JsonValue(std::move(obj));
}

PrepareRenameParams PrepareRenameParams::fromJson(const JsonValue& j) {
    PrepareRenameParams p;
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

JsonValue PrepareRenameResult::toJson() const {
    JsonValue::Object obj;
    obj["range"] = range.toJson();
    obj["placeholder"] = JsonValue(placeholder);
    return JsonValue(std::move(obj));
}

PrepareRenameResult PrepareRenameResult::fromJson(const JsonValue& j) {
    PrepareRenameResult r;
    if (auto* obj = asObject(j)) {
        if (auto* rng = findField(*obj, "range")) {
            r.range = Range::fromJson(*rng);
        }
        if (auto* ph = findField(*obj, "placeholder"); ph && ph->is_string()) {
            r.placeholder = std::get<std::string>(ph->value);
        }
    }
    return r;
}

JsonValue RenameParams::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object docObj;
    docObj["uri"] = JsonValue(textDocument.uri);
    obj["textDocument"] = JsonValue(std::move(docObj));
    obj["position"] = position.toJson();
    obj["newName"] = JsonValue(newName);
    return JsonValue(std::move(obj));
}

RenameParams RenameParams::fromJson(const JsonValue& j) {
    RenameParams p;
    if (auto* obj = asObject(j)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*docNode);
        }
        if (auto* posNode = findField(*obj, "position")) {
            p.position = Position::fromJson(*posNode);
        }
        if (auto* nnNode = findField(*obj, "newName"); nnNode && nnNode->is_string()) {
            p.newName = std::get<std::string>(nnNode->value);
        }
    }
    return p;
}

JsonValue ParameterInformation::toJson() const {
    JsonValue::Object obj;
    obj["label"] = JsonValue(label);
    if (documentation.has_value()) {
        obj["documentation"] = documentation->toJson();
    }
    return JsonValue(std::move(obj));
}

ParameterInformation ParameterInformation::fromJson(const JsonValue& j) {
    ParameterInformation pi;
    if (auto* obj = asObject(j)) {
        if (auto* l = findField(*obj, "label"); l && l->is_string()) {
            pi.label = std::get<std::string>(l->value);
        }
        if (auto* d = findField(*obj, "documentation")) {
            if (d->is_string()) {
                pi.documentation = MarkupContent{"markdown", std::get<std::string>(d->value)};
            } else if (d->is_object()) {
                pi.documentation = MarkupContent::fromJson(*d);
            }
        }
    }
    return pi;
}

JsonValue SignatureInformation::toJson() const {
    JsonValue::Object obj;
    obj["label"] = JsonValue(label);
    if (documentation.has_value()) {
        obj["documentation"] = documentation->toJson();
    }
    JsonValue::Array paramsArr;
    paramsArr.reserve(parameters.size());
    for (const auto& p : parameters) {
        paramsArr.push_back(p.toJson());
    }
    obj["parameters"] = JsonValue(std::move(paramsArr));
    if (activeParameter.has_value()) {
        obj["activeParameter"] = JsonValue(int64_t(*activeParameter));
    }
    return JsonValue(std::move(obj));
}

SignatureInformation SignatureInformation::fromJson(const JsonValue& j) {
    SignatureInformation si;
    if (auto* obj = asObject(j)) {
        if (auto* l = findField(*obj, "label"); l && l->is_string()) {
            si.label = std::get<std::string>(l->value);
        }
        if (auto* d = findField(*obj, "documentation")) {
            if (d->is_string()) {
                si.documentation = MarkupContent{"markdown", std::get<std::string>(d->value)};
            } else if (d->is_object()) {
                si.documentation = MarkupContent::fromJson(*d);
            }
        }
        if (auto* p = findField(*obj, "parameters"); p && p->is_array()) {
            for (const auto& item : std::get<JsonValue::Array>(p->value)) {
                si.parameters.push_back(ParameterInformation::fromJson(item));
            }
        }
        if (auto* ap = findField(*obj, "activeParameter")) {
            if (ap->is_int()) {
                si.activeParameter = static_cast<uint32_t>(std::get<int64_t>(ap->value));
            } else if (ap->is_number()) {
                si.activeParameter = static_cast<uint32_t>(std::get<double>(ap->value));
            }
        }
    }
    return si;
}

JsonValue SignatureHelp::toJson() const {
    JsonValue::Object obj;
    JsonValue::Array sigsArr;
    sigsArr.reserve(signatures.size());
    for (const auto& s : signatures) {
        sigsArr.push_back(s.toJson());
    }
    obj["signatures"] = JsonValue(std::move(sigsArr));
    obj["activeSignature"] = JsonValue(int64_t(activeSignature));
    obj["activeParameter"] = JsonValue(int64_t(activeParameter));
    return JsonValue(std::move(obj));
}

SignatureHelp SignatureHelp::fromJson(const JsonValue& j) {
    SignatureHelp sh;
    if (auto* obj = asObject(j)) {
        if (auto* s = findField(*obj, "signatures"); s && s->is_array()) {
            for (const auto& item : std::get<JsonValue::Array>(s->value)) {
                sh.signatures.push_back(SignatureInformation::fromJson(item));
            }
        }
        if (auto* as = findField(*obj, "activeSignature")) {
            if (as->is_int()) {
                sh.activeSignature = static_cast<uint32_t>(std::get<int64_t>(as->value));
            } else if (as->is_number()) {
                sh.activeSignature = static_cast<uint32_t>(std::get<double>(as->value));
            }
        }
        if (auto* ap = findField(*obj, "activeParameter")) {
            if (ap->is_int()) {
                sh.activeParameter = static_cast<uint32_t>(std::get<int64_t>(ap->value));
            } else if (ap->is_number()) {
                sh.activeParameter = static_cast<uint32_t>(std::get<double>(ap->value));
            }
        }
    }
    return sh;
}

JsonValue SignatureHelpParams::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object docObj;
    docObj["uri"] = JsonValue(textDocument.uri);
    obj["textDocument"] = JsonValue(std::move(docObj));
    obj["position"] = position.toJson();
    return JsonValue(std::move(obj));
}

SignatureHelpParams SignatureHelpParams::fromJson(const JsonValue& j) {
    SignatureHelpParams p;
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

SemanticTokensLegend SemanticTokensLegend::defaultLegend() {
    return SemanticTokensLegend{
        {
            "type",
            "class",
            "interface",
            "enum",
            "enumMember",
            "function",
            "method",
            "property",
            "variable",
            "parameter",
            "keyword",
            "string",
            "number",
            "operator",
            "comment"
        },
        {
            "declaration",
            "readonly",
            "static",
            "defaultLibrary"
        }
    };
}

JsonValue SemanticTokensLegend::toJson() const {
    JsonValue::Object obj;
    JsonValue::Array typesArr;
    typesArr.reserve(tokenTypes.size());
    for (const auto& t : tokenTypes) {
        typesArr.push_back(JsonValue(t));
    }
    obj["tokenTypes"] = JsonValue(std::move(typesArr));

    JsonValue::Array modsArr;
    modsArr.reserve(tokenModifiers.size());
    for (const auto& m : tokenModifiers) {
        modsArr.push_back(JsonValue(m));
    }
    obj["tokenModifiers"] = JsonValue(std::move(modsArr));
    return JsonValue(std::move(obj));
}

SemanticTokensLegend SemanticTokensLegend::fromJson(const JsonValue& j) {
    SemanticTokensLegend leg;
    if (auto* obj = asObject(j)) {
        if (auto* tt = findField(*obj, "tokenTypes"); tt && tt->is_array()) {
            for (const auto& item : std::get<JsonValue::Array>(tt->value)) {
                if (item.is_string()) {
                    leg.tokenTypes.push_back(std::get<std::string>(item.value));
                }
            }
        }
        if (auto* tm = findField(*obj, "tokenModifiers"); tm && tm->is_array()) {
            for (const auto& item : std::get<JsonValue::Array>(tm->value)) {
                if (item.is_string()) {
                    leg.tokenModifiers.push_back(std::get<std::string>(item.value));
                }
            }
        }
    }
    return leg;
}

JsonValue SemanticTokensParams::toJson() const {
    JsonValue::Object obj;
    JsonValue::Object docObj;
    docObj["uri"] = JsonValue(textDocument.uri);
    obj["textDocument"] = JsonValue(std::move(docObj));
    return JsonValue(std::move(obj));
}

SemanticTokensParams SemanticTokensParams::fromJson(const JsonValue& j) {
    SemanticTokensParams p;
    if (auto* obj = asObject(j)) {
        if (auto* docNode = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*docNode);
        }
    }
    return p;
}

JsonValue SemanticTokens::toJson() const {
    JsonValue::Object obj;
    if (resultId.has_value()) {
        obj["resultId"] = JsonValue(*resultId);
    }
    JsonValue::Array arr;
    arr.reserve(data.size());
    for (uint32_t val : data) {
        arr.push_back(JsonValue(int64_t(val)));
    }
    obj["data"] = JsonValue(std::move(arr));
    return JsonValue(std::move(obj));
}

SemanticTokens SemanticTokens::fromJson(const JsonValue& j) {
    SemanticTokens st;
    if (auto* obj = asObject(j)) {
        if (auto* rid = findField(*obj, "resultId"); rid && rid->is_string()) {
            st.resultId = std::get<std::string>(rid->value);
        }
        if (auto* d = findField(*obj, "data"); d && d->is_array()) {
            for (const auto& item : std::get<JsonValue::Array>(d->value)) {
                if (item.is_int()) {
                    st.data.push_back(static_cast<uint32_t>(std::get<int64_t>(item.value)));
                } else if (item.is_number()) {
                    st.data.push_back(static_cast<uint32_t>(std::get<double>(item.value)));
                }
            }
        }
    }
    return st;
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

FormattingOptions FormattingOptions::fromJson(const JsonValue& j) {
    FormattingOptions opt;
    if (auto* obj = asObject(j)) {
        if (auto* t = findField(*obj, "tabSize"); t && t->is_int()) {
            opt.tabSize = static_cast<uint32_t>(std::get<int64_t>(t->value));
        }
        if (auto* s = findField(*obj, "insertSpaces"); s && s->is_bool()) {
            opt.insertSpaces = std::get<bool>(s->value);
        }
        if (auto* w = findField(*obj, "trimTrailingWhitespace"); w && w->is_bool()) {
            opt.trimTrailingWhitespace = std::get<bool>(w->value);
        }
        if (auto* n = findField(*obj, "insertFinalNewline"); n && n->is_bool()) {
            opt.insertFinalNewline = std::get<bool>(n->value);
        }
        if (auto* tn = findField(*obj, "trimFinalNewlines"); tn && tn->is_bool()) {
            opt.trimFinalNewlines = std::get<bool>(tn->value);
        }
    }
    return opt;
}

DocumentFormattingParams DocumentFormattingParams::fromJson(const JsonValue& j) {
    DocumentFormattingParams p;
    if (auto* obj = asObject(j)) {
        if (auto* td = findField(*obj, "textDocument")) {
            p.textDocument = TextDocumentIdentifier::fromJson(*td);
        }
        if (auto* o = findField(*obj, "options")) {
            p.options = FormattingOptions::fromJson(*o);
        }
    }
    return p;
}

JsonValue TextEdit::toJson() const {
    JsonValue::Object obj;
    obj["range"] = range.toJson();
    obj["newText"] = JsonValue(newText);
    return JsonValue(std::move(obj));
}

TextEdit TextEdit::fromJson(const JsonValue& j) {
    TextEdit edit;
    if (auto* obj = asObject(j)) {
        if (auto* r = findField(*obj, "range")) {
            edit.range = Range::fromJson(*r);
        }
        if (auto* t = findField(*obj, "newText"); t && t->is_string()) {
            edit.newText = std::get<std::string>(t->value);
        }
    }
    return edit;
}

JsonValue WorkspaceEdit::toJson() const {
    JsonValue::Object changesObj;
    for (const auto& [uri, edits] : changes) {
        JsonValue::Array arr;
        arr.reserve(edits.size());
        for (const auto& edit : edits) {
            arr.push_back(edit.toJson());
        }
        changesObj[uri] = JsonValue(std::move(arr));
    }
    JsonValue::Object obj;
    obj["changes"] = JsonValue(std::move(changesObj));
    return JsonValue(std::move(obj));
}

WorkspaceEdit WorkspaceEdit::fromJson(const JsonValue& j) {
    WorkspaceEdit edit;
    if (auto* obj = asObject(j)) {
        if (auto* changesNode = findField(*obj, "changes")) {
            if (auto* chObj = asObject(*changesNode)) {
                for (const auto& [uri, editList] : *chObj) {
                    if (auto* arr = asArray(editList)) {
                        for (const auto& item : *arr) {
                            edit.changes[uri].push_back(TextEdit::fromJson(item));
                        }
                    }
                }
            }
        }
    }
    return edit;
}

CodeActionContext CodeActionContext::fromJson(const JsonValue& /*j*/) {
    CodeActionContext ctx;
    return ctx;
}

CodeActionParams CodeActionParams::fromJson(const JsonValue& j) {
    CodeActionParams params;
    if (auto* obj = asObject(j)) {
        if (auto* doc = findField(*obj, "textDocument")) {
            params.textDocument = TextDocumentIdentifier::fromJson(*doc);
        }
        if (auto* r = findField(*obj, "range")) {
            params.range = Range::fromJson(*r);
        }
        if (auto* ctx = findField(*obj, "context")) {
            params.context = CodeActionContext::fromJson(*ctx);
        }
    }
    return params;
}

JsonValue CodeAction::toJson() const {
    JsonValue::Object obj;
    obj["title"] = JsonValue(title);
    obj["kind"] = JsonValue(kind);
    obj["isPreferred"] = JsonValue(isPreferred);

    if (edit.has_value()) {
        obj["edit"] = edit->toJson();
    }

    if (!diagnostics.empty()) {
        JsonValue::Array diagArr;
        diagArr.reserve(diagnostics.size());
        for (const auto& d : diagnostics) {
            diagArr.push_back(d.toJson());
        }
        obj["diagnostics"] = JsonValue(std::move(diagArr));
    }

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
