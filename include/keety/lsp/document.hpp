#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <keety/lsp/protocol.hpp>

namespace keety::lsp {

/// Tracks live open text documents in memory with line offset tables for rapid Position <-> Offset translation.
class Document {
public:
    Document(std::string uri, std::string languageId, int32_t version, std::string text);

    const std::string& uri() const noexcept { return uri_; }
    const std::string& languageId() const noexcept { return languageId_; }
    int32_t version() const noexcept { return version_; }
    const std::string& text() const noexcept { return text_; }

    void update(int32_t newVersion, std::string newText);

    /// Translates a 0-indexed LSP Position (line, character UTF-8) to a byte offset.
    std::optional<size_t> positionToOffset(Position pos) const;

    /// Translates a byte offset to a 0-indexed LSP Position.
    Position offsetToPosition(size_t offset) const;

    size_t lineCount() const noexcept { return lineOffsets_.size(); }

private:
    void rebuildLineOffsets();

    std::string uri_;
    std::string languageId_;
    int32_t version_;
    std::string text_;
    std::vector<size_t> lineOffsets_; // Byte offset where each line starts
};

/// Thread-safe in-memory store of open documents managed by LSP client notifications.
class DocumentManager {
public:
    DocumentManager() = default;

    void openDocument(const TextDocumentItem& item);
    void updateDocument(const std::string& uri, int32_t version, const std::string& newText);
    void closeDocument(const std::string& uri);

    std::optional<Document> getDocument(const std::string& uri) const;
    bool hasDocument(const std::string& uri) const;
    std::vector<std::string> openUris() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Document> docs_;
};

} // namespace keety::lsp
