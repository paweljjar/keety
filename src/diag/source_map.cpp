#include <keety/diag/source_map.hpp>

#include <algorithm>
#include <sstream>

namespace keety::diag {

namespace {

bool paths_match(std::string_view p1, std::string_view p2) {
    if (p1 == p2) return true;
    if (p1.ends_with(p2) && (p1.size() == p2.size() || p1[p1.size() - p2.size() - 1] == '/' || p1[p1.size() - p2.size() - 1] == '\\')) {
        return true;
    }
    if (p2.ends_with(p1) && (p2.size() == p1.size() || p2[p2.size() - p1.size() - 1] == '/' || p2[p2.size() - p1.size() - 1] == '\\')) {
        return true;
    }
    return false;
}

} // namespace

void SourceMap::addMapping(uint32_t genLine, uint32_t genCol,
                           std::string sourceFile, uint32_t srcLine, uint32_t srcCol,
                           SourceSpan span, std::string symbolName) {
    if (genLine == 0 || sourceFile.empty() || srcLine == 0) {
        return;
    }

    SourceMapEntry entry;
    entry.generatedLine = genLine;
    entry.generatedColumn = genCol;
    entry.sourceFile = std::move(sourceFile);
    entry.sourceLine = srcLine;
    entry.sourceColumn = srcCol;
    entry.sourceSpan = span;
    entry.symbolName = std::move(symbolName);

    // Maintain ordered entries by generatedLine
    auto it = std::lower_bound(entries_.begin(), entries_.end(), genLine,
        [](const SourceMapEntry& e, uint32_t line) {
            return e.generatedLine < line;
        });

    if (it != entries_.end() && it->generatedLine == genLine) {
        // Update existing line mapping
        *it = std::move(entry);
    } else {
        entries_.insert(it, std::move(entry));
    }
}

std::optional<SourceMapEntry> SourceMap::findSourceLocation(
    uint32_t genLine, uint32_t genCol) const {
    if (entries_.empty() || genLine == 0) {
        return std::nullopt;
    }

    // Find first entry with generatedLine > genLine
    auto it = std::upper_bound(entries_.begin(), entries_.end(), genLine,
        [](uint32_t line, const SourceMapEntry& e) {
            return line < e.generatedLine;
        });

    if (it == entries_.begin()) {
        // genLine is before our first mapped line
        return std::nullopt;
    }

    // Previous entry is the best match
    const auto& match = *(--it);
    if (match.generatedLine == genLine) {
        SourceMapEntry result = match;
        if (genCol > 0) result.generatedColumn = genCol;
        return result;
    }

    // Line is between match and subsequent entry: derive approximate source line
    uint32_t line_offset = genLine - match.generatedLine;
    SourceMapEntry derived = match;
    derived.generatedLine = genLine;
    derived.generatedColumn = genCol;
    derived.sourceLine = match.sourceLine + line_offset;
    return derived;
}

std::optional<SourceMapEntry> SourceMap::findEntryForSourceLine(
    std::string_view sourceFile, uint32_t srcLine) const {
    if (entries_.empty()) {
        return std::nullopt;
    }

    const SourceMapEntry* best = nullptr;

    for (const auto& entry : entries_) {
        if (!paths_match(entry.sourceFile, sourceFile)) {
            continue;
        }

        if (entry.sourceLine == srcLine) {
            return entry;
        }

        if (entry.sourceLine < srcLine) {
            if (!best || entry.sourceLine > best->sourceLine) {
                best = &entry;
            }
        }
    }

    if (best) {
        SourceMapEntry derived = *best;
        uint32_t offset = srcLine - best->sourceLine;
        derived.generatedLine += offset;
        derived.sourceLine = srcLine;
        return derived;
    }

    return std::nullopt;
}

SourceMap SourceMap::parseFromCpp(std::string_view cppSource) {
    SourceMap map;
    std::istringstream stream{std::string(cppSource)};
    std::string line;
    uint32_t gen_line = 0;

    std::string current_file;
    uint32_t current_src_line = 0;
    bool has_directive = false;

    while (std::getline(stream, line)) {
        gen_line++;

        // Trim leading spaces
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.rfind("#line", start) == start) {
            std::string_view rest = std::string_view(line).substr(start + 5);
            size_t num_start = rest.find_first_not_of(" \t");
            if (num_start != std::string_view::npos) {
                size_t num_end = rest.find_first_of(" \t\r\n\"", num_start);
                std::string num_str(rest.substr(num_start, num_end == std::string_view::npos ? rest.size() : num_end - num_start));
                try {
                    current_src_line = static_cast<uint32_t>(std::stoul(num_str));
                    size_t quote_start = rest.find('"', num_start);
                    if (quote_start != std::string_view::npos) {
                        size_t quote_end = rest.find('"', quote_start + 1);
                        if (quote_end != std::string_view::npos) {
                            current_file = std::string(rest.substr(quote_start + 1, quote_end - quote_start - 1));
                        }
                    }
                    has_directive = true;
                    continue; // #line line itself
                } catch (...) {
                    // Ignore parse error
                }
            }
        }

        if (has_directive && !current_file.empty() && current_src_line > 0) {
            map.addMapping(gen_line, 1, current_file, current_src_line++, 1);
        }
    }

    return map;
}

} // namespace keety::diag

