#include <keety/diag/diagnostic_emitter.hpp>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace keety::diag {

namespace {

// ANSI escape sequences
constexpr const char* ANSI_RESET = "\033[0m";
constexpr const char* ANSI_BOLD = "\033[1m";
constexpr const char* ANSI_RED = "\033[1;31m";
constexpr const char* ANSI_YELLOW = "\033[1;33m";
constexpr const char* ANSI_BLUE = "\033[1;34m";
constexpr const char* ANSI_CYAN = "\033[1;36m";
constexpr const char* ANSI_GREEN = "\033[1;32m";

std::string_view severityToString(DiagnosticSeverity sev) noexcept {
    switch (sev) {
    case DiagnosticSeverity::Error:
        return "error";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Note:
        return "note";
    case DiagnosticSeverity::Help:
        return "help";
    }
    return "error";
}

} // namespace

DiagnosticEmitter::DiagnosticEmitter(const SourceManager& sourceManager, ColorMode colorMode)
    : sourceManager_(sourceManager), colorMode_(colorMode) {}

bool DiagnosticEmitter::shouldUseColor() const noexcept {
    const char* noColor = std::getenv("NO_COLOR");
    if (noColor && *noColor != '\0') {
        const char* clicolorForce = std::getenv("CLICOLOR_FORCE");
        if (!clicolorForce || std::string_view(clicolorForce) == "0") {
            return false;
        }
    }

    if (colorMode_ == ColorMode::Always) {
        return true;
    }
    if (colorMode_ == ColorMode::Never) {
        return false;
    }

    // ColorMode::Auto
    const char* clicolorForce = std::getenv("CLICOLOR_FORCE");
    if (clicolorForce && std::string_view(clicolorForce) != "0") {
        return true;
    }

    const char* clicolor = std::getenv("CLICOLOR");
    if (clicolor && std::string_view(clicolor) == "0") {
        return false;
    }

#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

void DiagnosticEmitter::report(Diagnostic diag) {
    if (diag.severity == DiagnosticSeverity::Error) {
        ++errorCount_;
    } else if (diag.severity == DiagnosticSeverity::Warning) {
        ++warningCount_;
    }
    diagnostics_.push_back(std::move(diag));
}

void DiagnosticEmitter::emit(const Diagnostic& diag, std::ostream& os) const {
    os << format(diag);
}

std::string DiagnosticEmitter::format(const Diagnostic& diag) const {
    const bool useColor = shouldUseColor();

    std::ostringstream ss;

    // 1. Header: error[E4001]: <message>
    if (useColor) {
        if (diag.severity == DiagnosticSeverity::Error) {
            ss << ANSI_RED;
        } else if (diag.severity == DiagnosticSeverity::Warning) {
            ss << ANSI_YELLOW;
        } else {
            ss << ANSI_CYAN;
        }
    }

    ss << severityToString(diag.severity);
    if (!diag.code.empty()) {
        ss << "[" << diag.code << "]";
    }
    ss << ": ";

    if (useColor) {
        ss << ANSI_RESET << ANSI_BOLD;
    }
    ss << diag.message << "\n";
    if (useColor) {
        ss << ANSI_RESET;
    }

    // Determine primary location & file
    SourceLocation loc = diag.location;
    if (!loc.isValid() && !diag.labels.empty()) {
        loc = diag.labels.front().span.start;
    }

    // Collect all unique FileIds present in location and labels
    std::vector<FileId> fileIds;
    if (loc.isValid()) {
        fileIds.push_back(loc.fileId);
    }
    for (const auto& lbl : diag.labels) {
        if (lbl.span.isValid()) {
            FileId fid = lbl.span.getFileId();
            if (std::find(fileIds.begin(), fileIds.end(), fid) == fileIds.end()) {
                fileIds.push_back(fid);
            }
        }
    }

    if (fileIds.empty()) {
        // If no source buffer or location, just render footers if any
        for (const auto& footer : diag.footers) {
            if (footer.kind == DiagnosticFooter::Kind::Note) {
                if (useColor) ss << ANSI_CYAN;
                ss << " = note: ";
            } else {
                if (useColor) ss << ANSI_GREEN;
                ss << " = help: ";
            }
            if (useColor) ss << ANSI_RESET;
            ss << footer.message << "\n";
        }
        return ss.str();
    }

    bool isFirstFile = true;
    size_t globalGutterWidth = 2;
    for (FileId fid : fileIds) {
        const SourceBuffer* buffer = sourceManager_.getBuffer(fid);
        if (!buffer) continue;

        // Collect labels for this buffer
        std::vector<DiagnosticLabel> fileLabels;
        for (const auto& lbl : diag.labels) {
            if (lbl.span.isValid() && lbl.span.getFileId() == fid) {
                DiagnosticLabel resolved = lbl;
                if (resolved.span.start.line == 0 || resolved.span.start.column == 0) {
                    resolved.span.start = buffer->getLocationAtOffset(lbl.span.start.offset);
                }
                if (resolved.span.end.line == 0 || resolved.span.end.column == 0) {
                    resolved.span.end = buffer->getLocationAtOffset(lbl.span.end.offset);
                }
                fileLabels.push_back(resolved);
            }
        }

        if (fileLabels.empty() && fid == loc.fileId && loc.isValid()) {
            SourceLocation resolvedLoc = (loc.line > 0 && loc.column > 0) ? loc : buffer->getLocationAtOffset(loc.offset);
            SourceSpan dummySpan{resolvedLoc, SourceLocation{resolvedLoc.fileId, resolvedLoc.offset + 1, resolvedLoc.line, resolvedLoc.column + 1}};
            fileLabels.push_back(DiagnosticLabel{dummySpan, "", true});
        }

        if (fileLabels.empty()) continue;

        // Determine gutter width based on maximum line number we will print for this file
        uint32_t primaryLine = (loc.line > 0) ? loc.line : buffer->getLocationAtOffset(loc.offset).line;
        uint32_t maxLine = (fid == loc.fileId) ? primaryLine : fileLabels.front().span.start.line;
        for (const auto& lbl : fileLabels) {
            maxLine = std::max(maxLine, lbl.span.start.line);
            maxLine = std::max(maxLine, lbl.span.end.line);
        }
        const size_t gutterWidth = std::max<size_t>(2, std::to_string(maxLine).length());
        globalGutterWidth = std::max(globalGutterWidth, gutterWidth);

        SourceLocation headerLoc;
        if (fid == loc.fileId && loc.isValid()) {
            headerLoc = (loc.line > 0 && loc.column > 0) ? loc : buffer->getLocationAtOffset(loc.offset);
        } else if (!fileLabels.empty()) {
            headerLoc = fileLabels.front().span.start;
        }

        // Header: --> for primary, ::: for secondary files
        if (useColor) ss << ANSI_BLUE;
        ss << std::string(gutterWidth, ' ') << (isFirstFile ? "--> " : "::: ");
        if (useColor) ss << ANSI_RESET;
        ss << buffer->getFilePath() << ":" << headerLoc.line << ":" << headerLoc.column << "\n";

        // Separator line: |
        if (useColor) ss << ANSI_BLUE;
        ss << std::string(gutterWidth, ' ') << " |\n";
        if (useColor) ss << ANSI_RESET;

        // Collect unique lines that have labels
        std::vector<uint32_t> linesToRender;
        for (const auto& lbl : fileLabels) {
            for (uint32_t l = lbl.span.start.line; l <= lbl.span.end.line; ++l) {
                if (std::find(linesToRender.begin(), linesToRender.end(), l) == linesToRender.end()) {
                    linesToRender.push_back(l);
                }
            }
        }
        std::sort(linesToRender.begin(), linesToRender.end());

        for (uint32_t lineNum : linesToRender) {
            std::string_view lineContent = buffer->getLineText(lineNum);

            // Source line display: "  42 | source line text..."
            if (useColor) ss << ANSI_BLUE;
            ss << std::setw(static_cast<int>(gutterWidth)) << lineNum << " | ";
            if (useColor) ss << ANSI_RESET;
            ss << lineContent << "\n";

            // Underline and squiggles
            std::vector<const DiagnosticLabel*> lineLabels;
            for (const auto& lbl : fileLabels) {
                if (lineNum >= lbl.span.start.line && lineNum <= lbl.span.end.line) {
                    lineLabels.push_back(&lbl);
                }
            }

            if (!lineLabels.empty()) {
                if (useColor) ss << ANSI_BLUE;
                ss << std::string(gutterWidth, ' ') << " | ";
                if (useColor) ss << ANSI_RESET;

                // Build squiggle line buffer
                size_t maxCol = lineContent.length() + 1;
                for (const auto* lbl : lineLabels) {
                    if (lbl->span.start.line == lineNum) {
                        maxCol = std::max<size_t>(maxCol, lbl->span.start.column + lbl->span.length() + 1);
                    }
                }

                std::string squiggles(maxCol, ' ');
                // Draw secondary labels first
                for (const auto* lbl : lineLabels) {
                    if (lbl->isPrimary) continue;
                    uint32_t startCol = (lbl->span.start.line == lineNum) ? lbl->span.start.column : 1;
                    uint32_t endCol = (lbl->span.end.line == lineNum) ? lbl->span.end.column : static_cast<uint32_t>(lineContent.length() + 1);
                    if (endCol <= startCol) {
                        endCol = startCol + 1;
                    }
                    size_t startIndex = (startCol > 0) ? (startCol - 1) : 0;
                    size_t endIndex = (endCol > 0) ? (endCol - 1) : 0;
                    for (size_t c = startIndex; c < endIndex && c < squiggles.size(); ++c) {
                        squiggles[c] = '-';
                    }
                }
                // Draw primary labels second (so they take precedence)
                for (const auto* lbl : lineLabels) {
                    if (!lbl->isPrimary) continue;
                    uint32_t startCol = (lbl->span.start.line == lineNum) ? lbl->span.start.column : 1;
                    uint32_t endCol = (lbl->span.end.line == lineNum) ? lbl->span.end.column : static_cast<uint32_t>(lineContent.length() + 1);
                    if (endCol <= startCol) {
                        endCol = startCol + 1;
                    }
                    size_t startIndex = (startCol > 0) ? (startCol - 1) : 0;
                    size_t endIndex = (endCol > 0) ? (endCol - 1) : 0;
                    for (size_t c = startIndex; c < endIndex && c < squiggles.size(); ++c) {
                        squiggles[c] = '^';
                    }
                }

                while (!squiggles.empty() && squiggles.back() == ' ') {
                    squiggles.pop_back();
                }

                if (useColor) {
                    ss << (diag.severity == DiagnosticSeverity::Error ? ANSI_RED : ANSI_YELLOW);
                }
                ss << squiggles;

                std::string inlineNotes;
                for (const auto* lbl : lineLabels) {
                    if (!lbl->message.empty() && lbl->span.start.line == lineNum) {
                        if (!inlineNotes.empty()) inlineNotes += ", ";
                        inlineNotes += lbl->message;
                    }
                }
                if (!inlineNotes.empty()) {
                    ss << " " << inlineNotes;
                }
                if (useColor) ss << ANSI_RESET;
                ss << "\n";
            }
        }

        // Closing pipe before next file or footers
        if (useColor) ss << ANSI_BLUE;
        ss << std::string(gutterWidth, ' ') << " |\n";
        if (useColor) ss << ANSI_RESET;

        isFirstFile = false;
    }

    // 5. Footers (notes and helps):
    //    = note: ...
    //    = help: ...
    for (const auto& footer : diag.footers) {
        if (useColor) ss << ANSI_BLUE;
        ss << std::string(globalGutterWidth, ' ') << " = ";
        if (footer.kind == DiagnosticFooter::Kind::Note) {
            if (useColor) ss << ANSI_CYAN;
            ss << "note: ";
        } else {
            if (useColor) ss << ANSI_GREEN;
            ss << "help: ";
        }
        if (useColor) ss << ANSI_RESET;

        const size_t prefixWidth = globalGutterWidth + 3 + (footer.kind == DiagnosticFooter::Kind::Note ? 6 : 6);
        std::istringstream msgStream(footer.message);
        std::string line;
        bool isFirst = true;
        while (std::getline(msgStream, line)) {
            if (!isFirst) {
                ss << std::string(prefixWidth, ' ');
            }
            ss << line << "\n";
            isFirst = false;
        }
    }

    return ss.str();
}

namespace {

std::string escapeJsonString(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

} // namespace

std::string DiagnosticEmitter::formatJson() const {
    if (diagnostics_.empty()) {
        return "[]\n";
    }

    std::ostringstream ss;
    ss << "[\n";

    for (size_t i = 0; i < diagnostics_.size(); ++i) {
        const auto& diag = diagnostics_[i];
        ss << "  {\n";
        ss << "    \"code\": \"" << escapeJsonString(diag.code) << "\",\n";
        ss << "    \"severity\": \"" << severityToString(diag.severity) << "\",\n";
        ss << "    \"message\": \"" << escapeJsonString(diag.message) << "\",\n";

        // Determine primary location
        SourceLocation loc = diag.location;
        if (!loc.isValid() && !diag.labels.empty()) {
            loc = diag.labels.front().span.start;
        }

        const SourceBuffer* buffer = loc.isValid() ? sourceManager_.getBuffer(loc.fileId) : nullptr;
        std::string filePath = buffer ? buffer->getFilePath() : "";

        uint32_t startLine = (loc.line > 0) ? loc.line : (buffer ? buffer->getLocationAtOffset(loc.offset).line : 1);
        uint32_t startCol = (loc.column > 0) ? loc.column : (buffer ? buffer->getLocationAtOffset(loc.offset).column : 1);
        uint32_t endLine = startLine;
        uint32_t endCol = startCol + 1;
        uint32_t length = 1;

        // If there are labels, find primary label's end location
        for (const auto& lbl : diag.labels) {
            if (lbl.isPrimary && lbl.span.isValid()) {
                const SourceBuffer* lblBuf = sourceManager_.getBuffer(lbl.span.getFileId());
                if (lblBuf) {
                    if (filePath.empty()) filePath = lblBuf->getFilePath();
                    SourceLocation s = (lbl.span.start.line > 0 && lbl.span.start.column > 0)
                        ? lbl.span.start : lblBuf->getLocationAtOffset(lbl.span.start.offset);
                    SourceLocation e = (lbl.span.end.line > 0 && lbl.span.end.column > 0)
                        ? lbl.span.end : lblBuf->getLocationAtOffset(lbl.span.end.offset);
                    startLine = s.line;
                    startCol = s.column;
                    endLine = e.line;
                    endCol = e.column;
                    length = lbl.span.length() > 0 ? lbl.span.length() : (endCol > startCol ? endCol - startCol : 1);
                    break;
                }
            }
        }

        ss << "    \"location\": {\n";
        ss << "      \"file\": \"" << escapeJsonString(filePath) << "\",\n";
        ss << "      \"line\": " << startLine << ",\n";
        ss << "      \"column\": " << startCol << ",\n";
        ss << "      \"endLine\": " << endLine << ",\n";
        ss << "      \"endColumn\": " << endCol << ",\n";
        ss << "      \"offset\": " << loc.offset << ",\n";
        ss << "      \"length\": " << length << "\n";
        ss << "    },\n";

        // Labels
        ss << "    \"labels\": [\n";
        for (size_t l = 0; l < diag.labels.size(); ++l) {
            const auto& lbl = diag.labels[l];
            const SourceBuffer* lblBuf = lbl.span.isValid() ? sourceManager_.getBuffer(lbl.span.getFileId()) : buffer;
            std::string lblFile = lblBuf ? lblBuf->getFilePath() : filePath;
            SourceLocation s = lblBuf ? ((lbl.span.start.line > 0 && lbl.span.start.column > 0) ? lbl.span.start : lblBuf->getLocationAtOffset(lbl.span.start.offset)) : loc;
            SourceLocation e = lblBuf ? ((lbl.span.end.line > 0 && lbl.span.end.column > 0) ? lbl.span.end : lblBuf->getLocationAtOffset(lbl.span.end.offset)) : loc;

            ss << "      {\n";
            ss << "        \"file\": \"" << escapeJsonString(lblFile) << "\",\n";
            ss << "        \"line\": " << s.line << ",\n";
            ss << "        \"column\": " << s.column << ",\n";
            ss << "        \"endLine\": " << e.line << ",\n";
            ss << "        \"endColumn\": " << e.column << ",\n";
            ss << "        \"message\": \"" << escapeJsonString(lbl.message) << "\",\n";
            ss << "        \"isPrimary\": " << (lbl.isPrimary ? "true" : "false") << "\n";
            ss << "      }" << (l + 1 < diag.labels.size() ? "," : "") << "\n";
        }
        ss << "    ],\n";

        // Notes and Helps
        std::vector<std::string> notes;
        std::vector<std::string> helps;
        for (const auto& f : diag.footers) {
            if (f.kind == DiagnosticFooter::Kind::Note) {
                notes.push_back(f.message);
            } else {
                helps.push_back(f.message);
            }
        }

        ss << "    \"notes\": [";
        for (size_t n = 0; n < notes.size(); ++n) {
            ss << (n == 0 ? "\n      \"" : "      \"") << escapeJsonString(notes[n]) << "\"" << (n + 1 < notes.size() ? ",\n" : "\n    ");
        }
        ss << "],\n";

        ss << "    \"helps\": [";
        for (size_t h = 0; h < helps.size(); ++h) {
            ss << (h == 0 ? "\n      \"" : "      \"") << escapeJsonString(helps[h]) << "\"" << (h + 1 < helps.size() ? ",\n" : "\n    ");
        }
        ss << "],\n";

        // Structured Suggestions
        struct JsonSuggestion {
            std::string type;
            std::string file;
            uint32_t startLine;
            uint32_t startColumn;
            uint32_t endLine;
            uint32_t endColumn;
            std::string replacement;
            std::string title;
        };
        std::vector<JsonSuggestion> suggestions;

        for (const auto& h : helps) {
            // Case 1: did you mean 'XYZ'?
            size_t dymPos = h.find("did you mean '");
            if (dymPos != std::string::npos) {
                size_t q1 = dymPos + 14;
                size_t q2 = h.find('\'', q1);
                if (q2 != std::string::npos) {
                    std::string repl = h.substr(q1, q2 - q1);
                    std::string title = (repl == "==") ? "Change '=' to '=='" : ("Change to '" + repl + "'");
                    suggestions.push_back(JsonSuggestion{
                        "replace",
                        filePath,
                        startLine,
                        startCol,
                        endLine,
                        endCol,
                        repl,
                        title
                    });
                }
            } else if (h.find("add 'import ") != std::string::npos) {
                size_t q1 = h.find("add '") + 5;
                size_t q2 = h.find('\'', q1);
                if (q2 != std::string::npos) {
                    std::string impStmt = h.substr(q1, q2 - q1) + "\n";
                    suggestions.push_back(JsonSuggestion{
                        "insert",
                        filePath,
                        1,
                        1,
                        1,
                        1,
                        impStmt,
                        "Add '" + h.substr(q1, q2 - q1) + "'"
                    });
                }
            } else if (h.find("add '(' to call '") != std::string::npos) {
                suggestions.push_back(JsonSuggestion{
                    "insert",
                    filePath,
                    startLine,
                    startCol,
                    startLine,
                    startCol,
                    "(",
                    "Add '(' before argument"
                });
            }
        }

        ss << "    \"suggestions\": [";
        for (size_t s = 0; s < suggestions.size(); ++s) {
            const auto& sug = suggestions[s];
            ss << (s == 0 ? "\n      {\n" : "      {\n");
            ss << "        \"type\": \"" << escapeJsonString(sug.type) << "\",\n";
            ss << "        \"file\": \"" << escapeJsonString(sug.file) << "\",\n";
            ss << "        \"startLine\": " << sug.startLine << ",\n";
            ss << "        \"startColumn\": " << sug.startColumn << ",\n";
            ss << "        \"endLine\": " << sug.endLine << ",\n";
            ss << "        \"endColumn\": " << sug.endColumn << ",\n";
            ss << "        \"replacement\": \"" << escapeJsonString(sug.replacement) << "\",\n";
            ss << "        \"title\": \"" << escapeJsonString(sug.title) << "\"\n";
            ss << "      }" << (s + 1 < suggestions.size() ? ",\n" : "\n    ");
        }
        ss << "]\n";

        ss << "  }" << (i + 1 < diagnostics_.size() ? "," : "") << "\n";
    }

    ss << "]\n";
    return ss.str();
}

void DiagnosticEmitter::emitJson(std::ostream& os) const {
    os << formatJson();
}

} // namespace keety::diag
