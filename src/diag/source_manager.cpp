#include <keety/diag/source_manager.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace keety::diag {

namespace {

/// Helper to check if byte is a UTF-8 continuation byte (10xxxxxx)
inline bool isUtf8Continuation(unsigned char byte) noexcept {
    return (byte & 0xC0) == 0x80;
}

} // namespace

SourceBuffer::SourceBuffer(FileId id, std::string filePath, std::string content)
    : id_(id), filePath_(std::move(filePath)), content_(std::move(content)) {
    computeLineOffsets();
}

void SourceBuffer::computeLineOffsets() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0); // Line 1 starts at byte 0

    const size_t len = content_.size();
    for (size_t i = 0; i < len; ++i) {
        if (content_[i] == '\n') {
            lineOffsets_.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

std::string_view SourceBuffer::getLineText(uint32_t line) const {
    if (line == 0 || line > lineOffsets_.size()) {
        return {};
    }

    const uint32_t startOffset = lineOffsets_[line - 1];
    uint32_t endOffset = (line < lineOffsets_.size())
                             ? lineOffsets_[line]
                             : static_cast<uint32_t>(content_.size());

    // Strip trailing newline characters ('\n' or '\r\n')
    while (endOffset > startOffset &&
           (content_[endOffset - 1] == '\n' || content_[endOffset - 1] == '\r')) {
        --endOffset;
    }

    if (startOffset >= content_.size()) {
        return {};
    }

    return std::string_view(content_.data() + startOffset, endOffset - startOffset);
}

SourceLocation SourceBuffer::getLocationAtOffset(uint32_t offset) const {
    if (offset > content_.size()) {
        offset = static_cast<uint32_t>(content_.size());
    }

    // Binary search for the line offset <= offset
    auto it = std::upper_bound(lineOffsets_.begin(), lineOffsets_.end(), offset);
    if (it != lineOffsets_.begin()) {
        --it;
    }

    const auto lineIndex = static_cast<uint32_t>(std::distance(lineOffsets_.begin(), it));
    const uint32_t line = lineIndex + 1; // 1-indexed
    const uint32_t lineStartOffset = *it;

    // Calculate column as UTF-8 character count (1-indexed)
    uint32_t column = 1;
    for (uint32_t i = lineStartOffset; i < offset; ++i) {
        auto byte = static_cast<unsigned char>(content_[i]);
        if (!isUtf8Continuation(byte)) {
            ++column;
        }
    }

    return SourceLocation{id_, offset, line, column};
}

std::optional<uint32_t> SourceBuffer::getOffsetAtLocation(uint32_t line, uint32_t column) const {
    if (line == 0 || line > lineOffsets_.size() || column == 0) {
        return std::nullopt;
    }

    const uint32_t lineStart = lineOffsets_[line - 1];
    const uint32_t lineEnd = (line < lineOffsets_.size())
                                 ? lineOffsets_[line]
                                 : static_cast<uint32_t>(content_.size());

    uint32_t currCol = 1;
    uint32_t offset = lineStart;

    while (offset < lineEnd && currCol < column) {
        // Stop before newline
        if (content_[offset] == '\n' || content_[offset] == '\r') {
            break;
        }

        // Advance offset
        ++offset;
        // Count UTF-8 lead bytes / single bytes
        while (offset < lineEnd && isUtf8Continuation(static_cast<unsigned char>(content_[offset]))) {
            ++offset;
        }
        ++currCol;
    }

    if (currCol == column) {
        return offset;
    }

    return std::nullopt;
}

std::string_view SourceBuffer::getSpanText(const SourceSpan& span) const {
    if (!span.isValid() || span.getFileId() != id_) {
        return {};
    }

    const uint32_t start = std::min(span.start.offset, static_cast<uint32_t>(content_.size()));
    const uint32_t end = std::min(span.end.offset, static_cast<uint32_t>(content_.size()));

    if (end <= start) {
        return {};
    }

    return std::string_view(content_.data() + start, end - start);
}

// -----------------------------------------------------------------------------
// SourceManager Implementation
// -----------------------------------------------------------------------------

std::optional<FileId> SourceManager::loadFile(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        canonicalPath = path;
    }

    // Check if file is already loaded
    if (const SourceBuffer* existing = getBufferByPath(canonicalPath)) {
        return existing->getId();
    }

    std::ifstream file(canonicalPath, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    auto fileId = static_cast<FileId>(buffers_.size() + 1);
    buffers_.push_back(
        std::make_unique<SourceBuffer>(fileId, canonicalPath.string(), std::move(content)));

    return fileId;
}

FileId SourceManager::addBuffer(std::string name, std::string content) {
    auto fileId = static_cast<FileId>(buffers_.size() + 1);
    buffers_.push_back(
        std::make_unique<SourceBuffer>(fileId, std::move(name), std::move(content)));
    return fileId;
}

const SourceBuffer* SourceManager::getBuffer(FileId fileId) const noexcept {
    if (fileId == INVALID_FILE_ID || fileId > buffers_.size()) {
        return nullptr;
    }
    return buffers_[fileId - 1].get();
}

const SourceBuffer* SourceManager::getBufferByPath(const std::filesystem::path& path) const {
    std::string pathStr = path.string();
    for (const auto& buf : buffers_) {
        if (buf->getFilePath() == pathStr) {
            return buf.get();
        }
    }
    return nullptr;
}

SourceLocation SourceManager::resolveLocation(FileId fileId, uint32_t offset) const {
    const SourceBuffer* buf = getBuffer(fileId);
    if (!buf) {
        return SourceLocation{};
    }
    return buf->getLocationAtOffset(offset);
}

std::string_view SourceManager::getSpanText(const SourceSpan& span) const {
    const SourceBuffer* buf = getBuffer(span.getFileId());
    if (!buf) {
        return {};
    }
    return buf->getSpanText(span);
}

std::string_view SourceManager::getLineText(const SourceLocation& loc) const {
    const SourceBuffer* buf = getBuffer(loc.fileId);
    if (!buf) {
        return {};
    }
    return buf->getLineText(loc.line);
}

} // namespace keety::diag
