#pragma once

#include <keety/diag/diagnostic.hpp>
#include <keety/diag/source_manager.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace keety::diag {

/// Controls ANSI color formatting
enum class ColorMode {
    Auto,   // Colored if stdout/stderr is a TTY
    Always, // Always emit ANSI escape sequences
    Never   // Never emit ANSI escape sequences (clean plain text)
};

/// Diagnostic emitter and renderer
class DiagnosticEmitter {
public:
    explicit DiagnosticEmitter(const SourceManager& sourceManager, ColorMode colorMode = ColorMode::Never);

    void setColorMode(ColorMode mode) noexcept { colorMode_ = mode; }
    [[nodiscard]] ColorMode getColorMode() const noexcept { return colorMode_; }
    [[nodiscard]] bool shouldUseColor() const noexcept;

    /// Formats a diagnostic into standard Rustc/Clang style string.
    [[nodiscard]] std::string format(const Diagnostic& diag) const;

    /// Emits formatted diagnostic directly to an output stream.
    void emit(const Diagnostic& diag, std::ostream& os) const;

    /// Adds a diagnostic to internal session storage.
    void report(Diagnostic diag);

    /// Checks if any errors were reported.
    [[nodiscard]] bool hasErrors() const noexcept { return errorCount_ > 0; }
    [[nodiscard]] size_t getErrorCount() const noexcept { return errorCount_; }
    [[nodiscard]] size_t getWarningCount() const noexcept { return warningCount_; }
    [[nodiscard]] const std::vector<Diagnostic>& getDiagnostics() const noexcept { return diagnostics_; }

    void clear() noexcept {
        diagnostics_.clear();
        errorCount_ = 0;
        warningCount_ = 0;
    }

private:
    const SourceManager& sourceManager_;
    ColorMode colorMode_{ColorMode::Never};
    std::vector<Diagnostic> diagnostics_;
    size_t errorCount_{0};
    size_t warningCount_{0};
};

} // namespace keety::diag
