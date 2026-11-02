#include <keety/lsp/document.hpp>

#include <algorithm>

namespace keety::lsp {

Document::Document(std::string uri, std::string languageId, int32_t version, std::string text)
    : uri_(std::move(uri)),
      languageId_(std::move(languageId)),
      version_(version),
      text_(std::move(text)) {
    rebuildLineOffsets();
}

void Document::update(int32_t newVersion, std::string newText) {
    version_ = newVersion;
    text_ = std::move(newText);
    rebuildLineOffsets();
}

void Document::rebuildLineOffsets() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0); // Line 0 begins at byte 0

    for (size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            lineOffsets_.push_back(i + 1);
        }
    }
}

std::optional<size_t> Document::positionToOffset(Position pos) const {
    if (pos.line >= lineOffsets_.size()) {
        return std::nullopt;
    }

    size_t lineStart = lineOffsets_[pos.line];
    size_t lineEnd = (pos.line + 1 < lineOffsets_.size())
        ? (lineOffsets_[pos.line + 1] - 1) // exclude '\n'
        : text_.size();

    size_t offset = lineStart + pos.character;
    if (offset > lineEnd) {
        offset = lineEnd; // clamp to end of line
    }
    return offset;
}

Position Document::offsetToPosition(size_t offset) const {
    Position pos{0, 0};
    if (text_.empty() || lineOffsets_.empty()) {
        return pos;
    }

    if (offset >= text_.size()) {
        offset = text_.size();
    }

    // Binary search for greatest lineStart <= offset
    auto it = std::upper_bound(lineOffsets_.begin(), lineOffsets_.end(), offset);
    if (it != lineOffsets_.begin()) {
        --it;
    }

    pos.line = static_cast<uint32_t>(std::distance(lineOffsets_.begin(), it));
    pos.character = static_cast<uint32_t>(offset - *it);
    return pos;
}

void DocumentManager::openDocument(const TextDocumentItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    docs_.insert_or_assign(item.uri, Document(item.uri, item.languageId, item.version, item.text));
}

void DocumentManager::updateDocument(const std::string& uri, int32_t version, const std::string& newText) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = docs_.find(uri);
    if (it != docs_.end()) {
        it->second.update(version, newText);
    } else {
        docs_.insert_or_assign(uri, Document(uri, "keet", version, newText));
    }
}

void DocumentManager::closeDocument(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    docs_.erase(uri);
}

std::optional<Document> DocumentManager::getDocument(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = docs_.find(uri);
    if (it != docs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool DocumentManager::hasDocument(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docs_.find(uri) != docs_.end();
}

std::vector<std::string> DocumentManager::openUris() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> uris;
    uris.reserve(docs_.size());
    for (const auto& [uri, _] : docs_) {
        uris.push_back(uri);
    }
    return uris;
}

} // namespace keety::lsp
