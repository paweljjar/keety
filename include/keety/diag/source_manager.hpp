#pragma once

#include <keety/diag/source_location.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::diag {

/// Stores the contents and line-offset index for a single source file buffer.
class SourceBuffer {
public:
    SourceBuffer(FileId id, std::string filePath, std::string content);

    [[nodiscard]] FileId getId() const noexcept { return id_; }
    [[nodiscard]] const std::string& getFilePath() const noexcept { return filePath_; }
    [[nodiscard]] std::string_view getContent() const noexcept { return content_; }
    [[nodiscard]] size_t getSize() const noexcept { return content_.size(); }
    [[nodiscard]] size_t getLineCount() const noexcept { return lineOffsets_.size(); }

    /// Returns the text of the specified line (1-indexed), without trailing '\n' or '\r'.
    [[nodiscard]] std::string_view getLineText(uint32_t line) const;

    /// Computes (line, column) for a given byte offset (0-indexed).
    /// Column takes UTF-8 character length into account (1-indexed).
    [[nodiscard]] SourceLocation getLocationAtOffset(uint32_t offset) const;

    /// Computes byte offset (0-indexed) for a given (line, column) (both 1-indexed).
    [[nodiscard]] std::optional<uint32_t> getOffsetAtLocation(uint32_t line, uint32_t column) const;

    /// Extracts source slice for a span.
    [[nodiscard]] std::string_view getSpanText(const SourceSpan& span) const;

private:
    void computeLineOffsets();

    FileId id_{INVALID_FILE_ID};
    std::string filePath_;
    std::string content_;
    std::vector<uint32_t> lineOffsets_; // Byte offset where each 1-indexed line begins
};

/// Thread-safe or hermetic manager for all loaded source buffers in a compilation session.
class SourceManager {
public:
    SourceManager() = default;
    ~SourceManager() = default;

    SourceManager(const SourceManager&) = delete;
    SourceManager& operator=(const SourceManager&) = delete;
    SourceManager(SourceManager&&) noexcept = default;
    SourceManager& operator=(SourceManager&&) noexcept = default;

    /// Loads a file from the filesystem. If already loaded, returns the existing FileId.
    std::optional<FileId> loadFile(const std::filesystem::path& path);

    /// Adds a virtual in-memory buffer (useful for tests, REPL, or generated code).
    FileId addBuffer(std::string name, std::string content);

    /// Retrieves a buffer by FileId, or nullptr if not found.
    [[nodiscard]] const SourceBuffer* getBuffer(FileId fileId) const noexcept;

    /// Retrieves a buffer by path, or nullptr if not found.
    [[nodiscard]] const SourceBuffer* getBufferByPath(const std::filesystem::path& path) const;

    /// Resolves a full SourceLocation with line/column computed from an offset.
    [[nodiscard]] SourceLocation resolveLocation(FileId fileId, uint32_t offset) const;

    /// Extracts source text for a given span.
    [[nodiscard]] std::string_view getSpanText(const SourceSpan& span) const;

    /// Extracts a line of text for a location.
    [[nodiscard]] std::string_view getLineText(const SourceLocation& loc) const;

private:
    std::vector<std::unique_ptr<SourceBuffer>> buffers_;
};

} // namespace keety::diag
