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

std::string LineDirectiveTracker::format_line_tracked(const diag::SourceSpan& span,
                                                      const diag::SourceManager* source_mgr,
                                                      std::string_view fallback_file,
                                                      std::string_view symbol) {
    uint32_t line = span.start.line;
    uint32_t col = span.start.column;
    std::string file(fallback_file);

    if (source_mgr && span.start.fileId != diag::INVALID_FILE_ID) {
        const auto* buf = source_mgr->getBuffer(span.start.fileId);
        if (buf) {
            file = buf->getFilePath();
            if (line == 0) {
                auto loc = buf->getLocationAtOffset(span.start.offset);
                line = loc.line;
                if (col == 0) col = loc.column;
            }
        }
    }

    if (line == 0 || file.empty()) {
        return "";
    }

    std::string norm_path = normalize_path(file);
    size_t id = tracked_records_.size();
    tracked_records_.push_back(TrackedLocation{id, norm_path, line, col, span, std::string(symbol)});

    std::string marker = "/*__KSM__:" + std::to_string(id) + "*/";

    if (enabled_) {
        std::string dir = format_line(line, norm_path);
        if (!dir.empty()) {
            return marker + dir;
        }
    }

    return marker + "\n";
}

std::string LineDirectiveTracker::finalize_source_map(std::string_view raw_emitted_cpp) {
    source_map_.clear();
    std::istringstream stream{std::string(raw_emitted_cpp)};
    std::string line;
    std::ostringstream out;

    uint32_t clean_line = 1;
    std::optional<TrackedLocation> pending_loc;

    while (std::getline(stream, line)) {
        size_t ksm_pos = line.find("/*__KSM__:");
        if (ksm_pos != std::string::npos) {
            size_t id_start = ksm_pos + 10;
            size_t id_end = line.find("*/", id_start);
            if (id_end != std::string::npos) {
                std::string id_str = line.substr(id_start, id_end - id_start);
                try {
                    size_t id = std::stoul(id_str);
                    if (id < tracked_records_.size()) {
                        pending_loc = tracked_records_[id];
                    }
                } catch (...) {}
                line.erase(ksm_pos, (id_end + 2) - ksm_pos);
            }
        }

        // If line is empty because only the marker was present (e.g. emit_line_directives disabled)
        if (line.empty()) {
            continue;
        }

        // If this line is a #line directive
        size_t non_ws = line.find_first_not_of(" \t");
        if (non_ws != std::string::npos && line.rfind("#line", non_ws) == non_ws) {
            out << line << "\n";
            clean_line++;
            // The mapping applies to the line right after #line
            if (pending_loc.has_value()) {
                source_map_.addMapping(clean_line, pending_loc->column, pending_loc->file,
                                       pending_loc->line, pending_loc->column,
                                       pending_loc->span, pending_loc->symbol);
                pending_loc.reset();
            }
            continue;
        }

        // Normal C++ code line
        if (pending_loc.has_value()) {
            source_map_.addMapping(clean_line, pending_loc->column, pending_loc->file,
                                   pending_loc->line, pending_loc->column,
                                   pending_loc->span, pending_loc->symbol);
            pending_loc.reset();
        }

        out << line << "\n";
        clean_line++;
    }

    return out.str();
}

void LineDirectiveTracker::reset() noexcept {
    last_file_.clear();
    last_line_ = 0;
}

} // namespace keety::emitter
