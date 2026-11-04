#include <keety/emitter/line_directive_tracker.hpp>

#include <algorithm>
#include <sstream>

namespace keety::emitter {

LineDirectiveTracker::LineDirectiveTracker(bool enabled)
    : enabled_(enabled) {}

std::string LineDirectiveTracker::normalize_path(std::string_view path) {
    if (path.empty()) {
        return "";
    }

    std::string result;
    result.reserve(path.size() + 4);

    for (char c : path) {
        if (c == '\\') {
            result += '/';
        } else if (c == '\"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }

    return result;
}

std::string LineDirectiveTracker::format_line(uint32_t line, std::string_view file_path) {
    if (!enabled_ || line == 0) {
        return "";
    }

    std::string norm_path = normalize_path(file_path);

    if (line == last_line_ && norm_path == last_file_) {
        return "";
    }

    last_line_ = line;
    last_file_ = norm_path;

    std::string directive = "#line " + std::to_string(line);
    if (!norm_path.empty()) {
        directive += " \"" + norm_path + "\"";
    }
    directive += "\n";
    return directive;
}

std::string LineDirectiveTracker::format_line(const diag::SourceSpan& span,
                                              const diag::SourceManager* source_mgr,
                                              std::string_view fallback_file) {
    if (!enabled_) {
        return "";
    }

    uint32_t line = span.start.line;
    std::string file(fallback_file);

    if (source_mgr && span.start.fileId != diag::INVALID_FILE_ID) {
        const auto* buf = source_mgr->getBuffer(span.start.fileId);
        if (buf) {
            file = buf->getFilePath();
            if (line == 0) {
                line = buf->getLocationAtOffset(span.start.offset).line;
            }
        }
    }

    if (line == 0) {
        return "";
    }

    return format_line(line, file);
}

void LineDirectiveTracker::reset() noexcept {
    last_file_.clear();
    last_line_ = 0;
}

} // namespace keety::emitter
