#include <keety/diag/diagnostic_translator.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace keety::diag {

namespace {

// Helper: Trim leading and trailing whitespace
std::string_view trim_sv(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

std::string trim(const std::string& s) {
    return std::string(trim_sv(s));
}

// Helper: Strip prefix if present
bool strip_prefix(std::string& s, std::string_view prefix) {
    if (s.rfind(prefix, 0) == 0) {
        s.erase(0, prefix.size());
        return true;
    }
    return false;
}

// Helper: Replace all occurrences of from with to
void replace_all(std::string& str, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// Splits top-level template arguments inside `<...>`
std::vector<std::string> split_template_arguments(std::string_view inner) {
    std::vector<std::string> args;
    int bracket_depth = 0;
    int paren_depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < inner.size(); ++i) {
        char c = inner[i];
        if (c == '<') {
            bracket_depth++;
        } else if (c == '>') {
            if (bracket_depth > 0) bracket_depth--;
        } else if (c == '(') {
            paren_depth++;
        } else if (c == ')') {
            if (paren_depth > 0) paren_depth--;
        } else if (c == ',' && bracket_depth == 0 && paren_depth == 0) {
            args.push_back(std::string(trim_sv(inner.substr(start, i - start))));
            start = i + 1;
        }
    }
    if (start < inner.size()) {
        args.push_back(std::string(trim_sv(inner.substr(start))));
    }
    return args;
}

// Cleans a quoted expression which could be a function signature (e.g. `fn(arg1, arg2)`) or a type
std::string clean_quoted_expression(std::string_view inner) {
    if (inner.empty()) return "";

    // Check if it's a function call / signature: ident(arg1, arg2)
    if (inner.back() == ')') {
        int depth = 0;
        size_t open_paren = std::string::npos;
        for (size_t i = inner.size(); i > 0; --i) {
            char c = inner[i - 1];
            if (c == ')') depth++;
            else if (c == '(') {
                depth--;
                if (depth == 0) {
                    open_paren = i - 1;
                    break;
                }
            }
        }
        if (open_paren != std::string::npos && open_paren > 0) {
            std::string fn_name = std::string(trim_sv(inner.substr(0, open_paren)));
            strip_prefix(fn_name, "keet::rt::");
            strip_prefix(fn_name, "::keet::rt::");
            strip_prefix(fn_name, "_keet_fn_");
            strip_prefix(fn_name, "_keet_var_");
            strip_prefix(fn_name, "_keet_");
            strip_prefix(fn_name, "keety_");

            std::string_view args_view = trim_sv(inner.substr(open_paren + 1, inner.size() - open_paren - 2));
            if (args_view.empty()) {
                return fn_name + "()";
            }
            auto args = split_template_arguments(args_view);
            std::string demangled_args;
            for (size_t k = 0; k < args.size(); ++k) {
                if (k > 0) demangled_args += ", ";
                demangled_args += DiagnosticTranslator::demangle_cpp_type(args[k]);
            }
            return fn_name + "(" + demangled_args + ")";
        }
    }

    return DiagnosticTranslator::demangle_cpp_type(inner);
}

} // namespace

std::string DiagnosticTranslator::demangle_cpp_type(std::string_view raw_cpp_type) {
    std::string s = trim(std::string(raw_cpp_type));
    if (s.empty()) return "";

    // Normalize inline STL namespace prefixes
    replace_all(s, "std::__1::", "std::");
    replace_all(s, "std::__cxx11::", "std::");
    replace_all(s, "::std::", "std::");

    // Strip const qualifiers and reference symbols
    bool is_const = false;
    if (strip_prefix(s, "const ")) {
        is_const = true;
    }
    while (!s.empty() && (s.back() == '&' || s.back() == ' ')) {
        s.pop_back();
    }
    s = trim(s);
    if (s.ends_with(" const")) {
        s.erase(s.size() - 6);
        is_const = true;
        s = trim(s);
    }

    // Strip leading struct/class/enum keywords
    strip_prefix(s, "struct ");
    strip_prefix(s, "class ");
    strip_prefix(s, "enum ");
    s = trim(s);

    // If starts with std::, check base template
    if (s.rfind("std::", 0) == 0) {
        s.erase(0, 5); // remove "std::"
    }

    // Check for template `<...>`
    size_t open_bracket = s.find('<');
    if (open_bracket != std::string::npos && s.back() == '>') {
        std::string base_name = s.substr(0, open_bracket);
        std::string_view inner = std::string_view(s).substr(open_bracket + 1, s.size() - open_bracket - 2);
        auto args = split_template_arguments(inner);

        if (base_name == "vector") {
            std::string elem = !args.empty() ? demangle_cpp_type(args[0]) : "unknown";
            return "[" + elem + "]";
        }
        if (base_name == "basic_string") {
            return "string";
        }
        if (base_name == "unordered_map" || base_name == "map") {
            std::string k = args.size() >= 1 ? demangle_cpp_type(args[0]) : "unknown";
            std::string v = args.size() >= 2 ? demangle_cpp_type(args[1]) : "unknown";
            return "[" + k + ": " + v + "]";
        }
        if (base_name == "variant") {
            if (args.empty()) return "void";
            std::string result;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) result += " | ";
                result += demangle_cpp_type(args[i]);
            }
            return result;
        }
        if (base_name == "optional") {
            std::string elem = !args.empty() ? demangle_cpp_type(args[0]) : "unknown";
            return elem + "?";
        }
        if (base_name == "shared_ptr" || base_name == "unique_ptr") {
            if (!args.empty()) {
                std::string inner_demangled = demangle_cpp_type(args[0]);
                if (inner_demangled == "keet::rt::Object" || inner_demangled == "Object") {
                    return "object";
                }
                return inner_demangled;
            }
            return "object";
        }
        if (base_name == "pair" && args.size() >= 2) {
            return "(" + demangle_cpp_type(args[0]) + ", " + demangle_cpp_type(args[1]) + ")";
        }
    }

    // Leaf types & primitives
    if (s == "string" || s == "string_view" || s == "basic_string") {
        return "string";
    }
    if (s == "int64_t" || s == "long long" || s == "long" || s == "int") {
        return "int";
    }
    if (s == "uint64_t" || s == "unsigned long long" || s == "unsigned long" || s == "unsigned int" || s == "uint") {
        return "uint";
    }
    if (s == "int32_t") {
        return "int32";
    }
    if (s == "uint32_t") {
        return "uint32";
    }
    if (s == "int16_t" || s == "short") {
        return "int16";
    }
    if (s == "uint16_t" || s == "unsigned short") {
        return "uint16";
    }
    if (s == "int8_t" || s == "signed char") {
        return "int8";
    }
    if (s == "uint8_t" || s == "unsigned char") {
        return "uint8";
    }
    if (s == "double" || s == "float") {
        return "float";
    }
    if (s == "bool") {
        return "bool";
    }
    if (s == "char") {
        return "char";
    }
    if (s == "void") {
        return "void";
    }
    if (s == "nullptr_t") {
        return "null";
    }

    // Strip KeetScript internal runtime namespaces and prefixes
    strip_prefix(s, "keet::rt::");
    strip_prefix(s, "keet::");
    strip_prefix(s, "::keet::rt::");
    strip_prefix(s, "::keet::");
    strip_prefix(s, "_keet_var_");
    strip_prefix(s, "_keet_fn_");
    strip_prefix(s, "_keet_");
    strip_prefix(s, "keety_");

    (void)is_const;
    return s;
}

std::string DiagnosticTranslator::clean_diagnostic_message(std::string_view raw_msg) {
    std::string msg(raw_msg);

    // Normalize internal namespace tags
    replace_all(msg, "std::__1::", "std::");
    replace_all(msg, "std::__cxx11::", "std::");
    replace_all(msg, "::std::", "std::");
    replace_all(msg, "keet::rt::", "");
    replace_all(msg, "::keet::rt::", "");
    replace_all(msg, "_keet_var_", "");
    replace_all(msg, "_keet_fn_", "");
    replace_all(msg, "_keet_", "");
    replace_all(msg, "keety_", "");

    // Process quoted expressions (e.g. 'std::vector<int64_t>', `foo()`)
    std::string result;
    size_t i = 0;
    while (i < msg.size()) {
        char quote = msg[i];
        if (quote == '\'' || quote == '`') {
            size_t close = std::string::npos;
            if (quote == '`') {
                size_t close_single = msg.find('\'', i + 1);
                size_t close_backtick = msg.find('`', i + 1);
                close = std::min(close_single, close_backtick);
            } else {
                close = msg.find('\'', i + 1);
            }
            if (close != std::string::npos) {
                std::string inner = msg.substr(i + 1, close - i - 1);
                std::string cleaned_inner = clean_quoted_expression(inner);
                result += "'" + cleaned_inner + "'";
                i = close + 1;
                continue;
            }
        }
        result += msg[i];
        ++i;
    }

    // Normalize standard error phrases
    replace_all(result, "no matching function for call to", "no matching function or method for call to");
    replace_all(result, "cannot convert", "type mismatch: cannot convert");

    return trim(result);
}

bool DiagnosticTranslator::is_internal_header(std::string_view file_path) {
    if (file_path.empty()) return false;

    // Check standard library header patterns
    if (file_path.find("/include/c++/") != std::string_view::npos ||
        file_path.find("/usr/include/c++/") != std::string_view::npos ||
        file_path.find("/libcxx/") != std::string_view::npos ||
        file_path.find("/v1/vector") != std::string_view::npos ||
        file_path.find("/v1/string") != std::string_view::npos ||
        file_path.find("/v1/variant") != std::string_view::npos ||
        file_path.find("/v1/utility") != std::string_view::npos ||
        file_path.find("/v1/type_traits") != std::string_view::npos ||
        file_path.find("/v1/memory") != std::string_view::npos) {
        return true;
    }

    // Internal standard library filenames
    const std::vector<std::string_view> internal_headers = {
        "vector", "string", "variant", "utility", "type_traits", "memory", "optional",
        "unordered_map", "map", "functional", "algorithm", "iostream", "ostream", "sstream",
        "xutility", "xtr1common", "xmemory", "xstring", "stl_vector.h", "stl_iterator.h",
        "stl_map.h", "stl_tree.h", "stl_algo.h", "__split_buffer", "basic_string.h",
        "runtime.hpp", "keet/runtime.hpp"
    };

    for (const auto& h : internal_headers) {
        if (file_path == h || file_path.ends_with("/" + std::string(h)) || file_path.ends_with("\\" + std::string(h))) {
            return true;
        }
    }

    return false;
}

bool DiagnosticTranslator::is_internal_noise_line(std::string_view line) {
    std::string_view l = trim_sv(line);
    if (l.empty()) return true;

    // Candidate template instantiation failures in STL internals
    if (l.find("in instantiation of template") != std::string_view::npos ||
        l.find("expanded from macro") != std::string_view::npos ||
        l.find("candidate template ignored") != std::string_view::npos ||
        l.find("candidate function template not viable") != std::string_view::npos ||
        l.find("candidate function not viable") != std::string_view::npos) {
        return true;
    }

    // Internal compiler note referencing std namespace
    if (l.find("note: ") != std::string_view::npos) {
        if (l.find("std::") != std::string_view::npos ||
            l.find("std::__1::") != std::string_view::npos ||
            l.find("keet::rt::") != std::string_view::npos) {
            return true;
        }
    }

    return false;
}

std::vector<RawDiagnostic> DiagnosticTranslator::parse_clang_gcc_output(std::string_view raw_output) {
    std::vector<RawDiagnostic> diagnostics;
    std::istringstream stream{std::string(raw_output)};
    std::string line;

    // Matches: <file>:<line>:<col>: (error|warning|note|fatal error): <message>
    // or:      <file>:<line>: (error|warning|note|fatal error): <message>
    static const std::regex diag_regex(
        R"(^([^:\r\n]+):(\d+):(?:(\d+):)?\s*(error|warning|note|fatal error):\s*(.+)$)",
        std::regex::optimize);

    RawDiagnostic* current_diag = nullptr;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, diag_regex)) {
            std::string file = match[1].str();
            uint32_t line_num = static_cast<uint32_t>(std::stoul(match[2].str()));
            uint32_t col_num = match[3].matched ? static_cast<uint32_t>(std::stoul(match[3].str())) : 0;
            std::string sev_str = match[4].str();
            std::string message = match[5].str();

            DiagnosticSeverity sev = DiagnosticSeverity::Error;
            if (sev_str == "warning") {
                sev = DiagnosticSeverity::Warning;
            } else if (sev_str == "note") {
                sev = DiagnosticSeverity::Note;
            }

            if (sev == DiagnosticSeverity::Note) {
                // Attach note to active diagnostic if not STL noise
                if (current_diag && !is_internal_header(file) && !is_internal_noise_line(line)) {
                    current_diag->notes.push_back(clean_diagnostic_message(message));
                }
                continue;
            }

            // Skip errors/warnings originating purely from internal headers
            if (is_internal_header(file)) {
                current_diag = nullptr;
                continue;
            }

            RawDiagnostic diag;
            diag.file = std::move(file);
            diag.line = line_num;
            diag.column = col_num;
            diag.severity = sev;
            diag.rawMessage = message;
            diag.cleanMessage = clean_diagnostic_message(message);

            diagnostics.push_back(std::move(diag));
            current_diag = &diagnostics.back();
        } else if (current_diag && !line.empty()) {
            // Check if this continuation line contains an actionable suggestion
            std::string_view trimmed = trim_sv(line);
            if (trimmed.rfind("did you mean", 0) != std::string_view::npos ||
                trimmed.rfind("suggestion:", 0) != std::string_view::npos) {
                current_diag->notes.push_back(clean_diagnostic_message(trimmed));
            }
        }
    }

    return diagnostics;
}

std::vector<RawDiagnostic> DiagnosticTranslator::parse_msvc_output(std::string_view raw_output) {
    std::vector<RawDiagnostic> diagnostics;
    std::istringstream stream{std::string(raw_output)};
    std::string line;

    // Matches: <file>(<line>,<col>): (error|warning) <Cxxxx>: <message>
    // or:      <file>(<line>): (error|warning) <Cxxxx>: <message>
    static const std::regex msvc_regex(
        R"(^([^(:\r\n]+)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning)\s*(C\d+):\s*(.+)$)",
        std::regex::optimize);

    RawDiagnostic* current_diag = nullptr;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, msvc_regex)) {
            std::string file = match[1].str();
            uint32_t line_num = static_cast<uint32_t>(std::stoul(match[2].str()));
            uint32_t col_num = match[3].matched ? static_cast<uint32_t>(std::stoul(match[3].str())) : 0;
            std::string sev_str = match[4].str();
            std::string code = match[5].str();
            std::string message = match[6].str();

            DiagnosticSeverity sev = (sev_str == "warning") ? DiagnosticSeverity::Warning : DiagnosticSeverity::Error;

            if (is_internal_header(file)) {
                current_diag = nullptr;
                continue;
            }

            RawDiagnostic diag;
            diag.file = std::move(file);
            diag.line = line_num;
            diag.column = col_num;
            diag.severity = sev;
            diag.code = std::move(code);
            diag.rawMessage = message;
            diag.cleanMessage = clean_diagnostic_message(message);

            diagnostics.push_back(std::move(diag));
            current_diag = &diagnostics.back();
        } else if (current_diag && !line.empty()) {
            std::string_view trimmed = trim_sv(line);
            if (trimmed.rfind("see reference to", 0) == std::string_view::npos &&
                trimmed.rfind("while compiling", 0) == std::string_view::npos) {
                if (!is_internal_noise_line(trimmed)) {
                    current_diag->notes.push_back(clean_diagnostic_message(trimmed));
                }
            }
        }
    }

    return diagnostics;
}

std::vector<RawDiagnostic> DiagnosticTranslator::parse_compiler_output(
    std::string_view raw_output,
    driver::CompilerKind kind) {
    if (kind == driver::CompilerKind::MSVC) {
        return parse_msvc_output(raw_output);
    }
    return parse_clang_gcc_output(raw_output);
}

ErrorCode DiagnosticTranslator::map_to_error_code(std::string_view clean_msg) {
    std::string lower(clean_msg);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    if (lower.find("cannot convert") != std::string::npos ||
        lower.find("no viable conversion") != std::string::npos ||
        lower.find("no matching function") != std::string::npos ||
        lower.find("no matching constructor") != std::string::npos ||
        lower.find("incompatible type") != std::string::npos) {
        return ErrorCode::TypeMismatch;
    }

    if (lower.find("undeclared identifier") != std::string::npos ||
        lower.find("not declared in this scope") != std::string::npos ||
        lower.find("unknown type name") != std::string::npos) {
        return ErrorCode::UndeclaredIdentifier;
    }

    if (lower.find("no member named") != std::string::npos ||
        lower.find("has no member") != std::string::npos ||
        lower.find("is not a member of") != std::string::npos) {
        return ErrorCode::InvalidMemberAccess;
    }

    if (lower.find("invalid cast") != std::string::npos ||
        lower.find("reinterpret_cast") != std::string::npos ||
        lower.find("static_cast") != std::string::npos) {
        return ErrorCode::InvalidTypeCast;
    }

    if (lower.find("template instantiation") != std::string::npos) {
        return ErrorCode::BackendTemplateInstantiation;
    }

    return ErrorCode::BackendCompilationError;
}

std::vector<Diagnostic> DiagnosticTranslator::translate(
    std::string_view raw_output,
    driver::CompilerKind kind) {
    auto raw_diags = parse_compiler_output(raw_output, kind);
    std::vector<Diagnostic> result;
    result.reserve(raw_diags.size());

    for (const auto& raw : raw_diags) {
        ErrorCode code = map_to_error_code(raw.cleanMessage);
        SourceLocation loc{};
        loc.line = raw.line;
        loc.column = raw.column;

        Diagnostic d = (raw.severity == DiagnosticSeverity::Warning)
            ? Diagnostic::makeWarning(std::string(getErrorCodeString(code)), raw.cleanMessage, loc)
            : Diagnostic::makeError(code, raw.cleanMessage, loc);

        for (const auto& note : raw.notes) {
            if (note.rfind("did you mean", 0) != std::string::npos ||
                note.rfind("suggestion", 0) != std::string::npos) {
                d.withHelp(note);
            } else {
                d.withNote(note);
            }
        }

        result.push_back(std::move(d));
    }

    return result;
}

} // namespace keety::diag
