#pragma once

#include <cstdint>
#include <string>

namespace keety::diag {

/// File identifier referencing a buffer managed by SourceManager.
using FileId = uint32_t;
inline constexpr FileId INVALID_FILE_ID = 0;

/// Represents a single point in a source file.
struct SourceLocation {
    FileId fileId{INVALID_FILE_ID};
    uint32_t offset{0};  // Byte offset from file start (0-indexed)
    uint32_t line{1};    // 1-indexed line number
    uint32_t column{1};  // 1-indexed column number (UTF-8 character or byte column)

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return fileId != INVALID_FILE_ID;
    }

    [[nodiscard]] constexpr bool operator==(const SourceLocation& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const SourceLocation& other) const noexcept = default;
};

/// Represents a contiguous span [start, end) in a source file.
struct SourceSpan {
    SourceLocation start{};
    SourceLocation end{};

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return start.isValid() && end.isValid() && start.fileId == end.fileId && start.offset <= end.offset;
    }

    [[nodiscard]] constexpr FileId getFileId() const noexcept {
        return start.fileId;
    }

    [[nodiscard]] constexpr uint32_t length() const noexcept {
        return end.offset >= start.offset ? (end.offset - start.offset) : 0;
    }

    /// Combines two spans into a single enclosing span covering both.
    [[nodiscard]] constexpr SourceSpan merge(const SourceSpan& other) const noexcept {
        if (!isValid()) return other;
        if (!other.isValid()) return *this;
        if (start.fileId != other.start.fileId) return *this;

        SourceLocation newStart = (start.offset <= other.start.offset) ? start : other.start;
        SourceLocation newEnd = (end.offset >= other.end.offset) ? end : other.end;
        return SourceSpan{newStart, newEnd};
    }

    [[nodiscard]] constexpr bool contains(const SourceLocation& loc) const noexcept {
        if (!isValid() || !loc.isValid() || start.fileId != loc.fileId) return false;
        return loc.offset >= start.offset && loc.offset < end.offset;
    }

    [[nodiscard]] constexpr bool operator==(const SourceSpan& other) const noexcept = default;
};

} // namespace keety::diag
