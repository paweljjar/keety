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

} // namespace keety::diag
