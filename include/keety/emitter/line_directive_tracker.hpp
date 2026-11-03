#pragma once

#include <keety/diag/source_location.hpp>
#include <keety/diag/source_manager.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace keety::emitter {

/// Tracks emitted source line directives (#line <line> "<file>") to guarantee
/// deduplication, hermetic formatting, and cross-platform path normalization.
class LineDirectiveTracker {
public:
    explicit LineDirectiveTracker(bool enabled = true);

    /// Normalizes path for C/C++ #line directives (e.g. converts Windows backslashes to forward slashes).
    [[nodiscard]] static std::string normalize_path(std::string_view path);

    /// Generates a #line directive if the file or line has changed since the last emission.
    /// Returns an empty string if disabled, line is 0, or if (file == last_file && line == last_line).
    [[nodiscard]] std::string format_line(uint32_t line, std::string_view file_path);

    /// Resolves span via SourceManager (or fallback_file) and formats a #line directive if changed.
    [[nodiscard]] std::string format_line(const diag::SourceSpan& span,
                                          const diag::SourceManager* source_mgr = nullptr,
                                          std::string_view fallback_file = "");

    /// Clears last emitted line and file state.
    void reset() noexcept;

    /// Enables or disables line directive emission.
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool is_enabled() const noexcept { return enabled_; }

    [[nodiscard]] uint32_t get_last_line() const noexcept { return last_line_; }
    [[nodiscard]] const std::string& get_last_file() const noexcept { return last_file_; }

private:
    bool enabled_{true};
    std::string last_file_{};
    uint32_t last_line_{0};
};

} // namespace keety::emitter
