#pragma once

#include <keety/diag/source_location.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace keety::diag {

/// Diagnostic severity levels
enum class DiagnosticSeverity {
    Error,
    Warning,
    Note,
    Help
};

/// Inline label attached to a specific source span (e.g. `^^^^ first mutable borrow occurs here`)
struct DiagnosticLabel {
    SourceSpan span;
    std::string message;
    bool isPrimary{true}; // Primary labels usually trigger main squiggles (^^^^), secondary (----)
};

/// Additional contextual note or help message attached to a diagnostic
struct DiagnosticFooter {
    enum class Kind {
        Note,
        Help
    };

    Kind kind{Kind::Note};
    std::string message;
};

/// Complete diagnostic message structure
struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string code; // e.g. "E4001"
    std::string message;
    SourceLocation location; // Primary location for file:line:col header
    std::vector<DiagnosticLabel> labels;
    std::vector<DiagnosticFooter> footers;

    Diagnostic() = default;

    static Diagnostic makeError(std::string code, std::string message, SourceLocation loc = {}) {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.code = std::move(code);
        d.message = std::move(message);
        d.location = loc;
        return d;
    }

    static Diagnostic makeWarning(std::string code, std::string message, SourceLocation loc = {}) {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Warning;
        d.code = std::move(code);
        d.message = std::move(message);
        d.location = loc;
        return d;
    }

    Diagnostic& withLabel(SourceSpan span, std::string labelMsg, bool isPrimary = true) {
        if (!location.isValid() && span.isValid()) {
            location = span.start;
        }
        labels.push_back(DiagnosticLabel{span, std::move(labelMsg), isPrimary});
        return *this;
    }

    Diagnostic& withNote(std::string noteMsg) {
        footers.push_back(DiagnosticFooter{DiagnosticFooter::Kind::Note, std::move(noteMsg)});
        return *this;
    }

    Diagnostic& withHelp(std::string helpMsg) {
        footers.push_back(DiagnosticFooter{DiagnosticFooter::Kind::Help, std::move(helpMsg)});
        return *this;
    }
};

} // namespace keety::diag
