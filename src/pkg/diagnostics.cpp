#include <keety/pkg/diagnostics.hpp>

namespace keety::pkg {

std::string codeString(ErrorCode code) {
    return "KAT" + std::to_string(static_cast<int>(code));
}

std::string Diagnostic::format() const {
    std::string out = "error[" + codeString(code) + "]: " + message;
    if (!hint.empty()) {
        out += "\n  hint: " + hint;
    }
    return out;
}

PkgError::PkgError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

PkgError::PkgError(ErrorCode code, std::string message, std::string hint)
    : PkgError(Diagnostic{code, std::move(message), std::move(hint)}) {}

} // namespace keety::pkg
