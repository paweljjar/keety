#pragma once

#include <keety/diag/source_location.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::diag {

/// A mapping between a generated C++ location and its originating KeetScript source location
struct SourceMapEntry {
    uint32_t generatedLine{0};      ///< 1-based line in generated C++ file
    uint32_t generatedColumn{0};    ///< 1-based column in generated C++ file
    std::string sourceFile;         ///< Path to original KeetScript source file (.keet)
    uint32_t sourceLine{0};         ///< 1-based line in original source file
    uint32_t sourceColumn{0};       ///< 1-based column in original source file
    SourceSpan sourceSpan{};        ///< Originating SourceSpan if known
    std::string symbolName{};       ///< Optional enclosing symbol name (function, class, variable)

    [[nodiscard]] bool isValid() const noexcept {
        return generatedLine > 0 && !sourceFile.empty() && sourceLine > 0;
    }
};

/// Bi-directional source map between generated C++ files and KeetScript sources
class SourceMap {
public:
    SourceMap() = default;

    /// Adds a location mapping between generated C++ and original .keet source
    void addMapping(uint32_t genLine, uint32_t genCol,
                    std::string sourceFile, uint32_t srcLine, uint32_t srcCol,
                    SourceSpan span = {}, std::string symbolName = "");

    /// Finds the closest preceding source mapping for a generated C++ line and column
    [[nodiscard]] std::optional<SourceMapEntry> findSourceLocation(
        uint32_t genLine, uint32_t genCol = 0) const;

    /// Finds an entry matching the given KeetScript source file and line
    [[nodiscard]] std::optional<SourceMapEntry> findEntryForSourceLine(
        std::string_view sourceFile, uint32_t srcLine) const;

    /// Parses `#line` directives from generated C++ source text to construct a source map
    [[nodiscard]] static SourceMap parseFromCpp(std::string_view cppSource);

    /// Returns all registered entries
    [[nodiscard]] const std::vector<SourceMapEntry>& getEntries() const noexcept { return entries_; }

    /// Checks if the source map is empty
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    /// Number of entries
    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }

    /// Clears all registered entries
    void clear() noexcept { entries_.clear(); }

private:
    std::vector<SourceMapEntry> entries_;
};

} // namespace keety::diag

