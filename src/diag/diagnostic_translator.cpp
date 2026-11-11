#include <keety/diag/diagnostic_translator.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define KEETY_HAS_CXXABI 1
#endif
#endif

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

std::string DiagnosticTranslator::demangle_symbol(std::string_view mangled_name) {
    std::string s = trim(std::string(mangled_name));
    if (s.empty()) return "";

    // 1. Try Itanium C++ demangling via __cxa_demangle if starts with _Z or __Z
#if defined(KEETY_HAS_CXXABI)
    if (s.rfind("_Z", 0) == 0 || s.rfind("__Z", 0) == 0) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(s.c_str(), nullptr, nullptr, &status);
        if (status == 0 && demangled) {
            std::string result(demangled);
            std::free(demangled);
            return clean_diagnostic_message(result);
        }
    }
#endif

    // 2. Strip KeetScript internal runtime namespaces and mangling prefixes
    strip_prefix(s, "keet::rt::");
    strip_prefix(s, "::keet::rt::");
    strip_prefix(s, "_keet_fn_");
    strip_prefix(s, "_keet_var_");
    strip_prefix(s, "_keet_");
    strip_prefix(s, "keet_fn_");
    strip_prefix(s, "keet_var_");
    strip_prefix(s, "keet_");
    strip_prefix(s, "keety_");

    // 3. Strip macOS / C calling convention leading underscore: _symbol -> symbol
    if (s.size() > 1 && s[0] == '_' && (std::isalpha(static_cast<unsigned char>(s[1])) || s[1] == '_')) {
        if (s.rfind("__builtin", 0) != 0 && s.rfind("__cxx", 0) != 0) {
            s.erase(0, 1);
        }
    }

    return clean_diagnostic_message(s);
}

std::string DiagnosticTranslator::suggest_package_for_symbol(std::string_view symbol) {
    if (symbol.empty()) return "";

    // SQLite
    if (symbol.rfind("sqlite3_", 0) == 0 || symbol.rfind("sqlite3", 0) == 0) {
        return "sqlite";
    }

    // OpenSSL / Crypto
    if (symbol.rfind("SSL_", 0) == 0 || symbol.rfind("TLS_", 0) == 0 ||
        symbol.rfind("CRYPTO_", 0) == 0 || symbol.rfind("EVP_", 0) == 0 ||
        symbol.rfind("OpenSSL_", 0) == 0 || symbol.rfind("X509_", 0) == 0 ||
        symbol.rfind("HMAC_", 0) == 0 || symbol.rfind("SHA256_", 0) == 0 ||
        symbol.rfind("BIO_", 0) == 0 || symbol.rfind("RSA_", 0) == 0) {
        return "openssl";
    }

    // libcurl
    if (symbol.rfind("curl_", 0) == 0) {
        return "libcurl";
    }

    // zlib
    if (symbol.rfind("deflate", 0) == 0 || symbol.rfind("inflate", 0) == 0 ||
        symbol.rfind("compress", 0) == 0 || symbol.rfind("uncompress", 0) == 0 ||
        symbol.rfind("crc32", 0) == 0 || symbol.rfind("adler32", 0) == 0 ||
        symbol.rfind("zlibVersion", 0) == 0) {
        return "zlib";
    }

    // Math library
    const std::vector<std::string_view> math_symbols = {
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh", "tanh",
        "sqrt", "pow", "exp", "log", "log10", "floor", "ceil", "fmod", "round", "hypot"
    };
    for (const auto& m : math_symbols) {
        if (symbol == m || symbol == std::string(m) + "f" || symbol == std::string(m) + "l") {
            return "math library (-lm)";
        }
    }

    // libuv
    if (symbol.rfind("uv_", 0) == 0) {
        return "libuv";
    }

    // pthreads
    if (symbol.rfind("pthread_", 0) == 0) {
        return "pthreads (-pthread)";
    }

    // PCRE
    if (symbol.rfind("pcre2_", 0) == 0 || symbol.rfind("pcre_", 0) == 0) {
        return "pcre2";
    }

    // SDL2 / GLFW
    if (symbol.rfind("SDL_", 0) == 0) {
        return "sdl2";
    }
    if (symbol.rfind("glfw", 0) == 0) {
        return "glfw";
    }

    // Vulkan / OpenGL
    if (symbol.rfind("vk", 0) == 0 && symbol.size() > 2 && std::isupper(static_cast<unsigned char>(symbol[2]))) {
        return "vulkan";
    }
    if (symbol.rfind("gl", 0) == 0 && symbol.size() > 2 && std::isupper(static_cast<unsigned char>(symbol[2]))) {
        return "opengl";
    }

    return "";
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

    if (lower.find("redefinition of") != std::string::npos ||
        lower.find("conflicting types for") != std::string::npos ||
        lower.find("already defined") != std::string::npos) {
        return ErrorCode::RedeclarationInSameScope;
    }

    if (lower.find("cannot convert") != std::string::npos ||
        lower.find("no viable conversion") != std::string::npos ||
        lower.find("no matching function") != std::string::npos ||
        lower.find("no matching constructor") != std::string::npos ||
        lower.find("incompatible type") != std::string::npos ||
        lower.find("no viable overloaded '='") != std::string::npos) {
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

    if (lower.find("cannot assign to") != std::string::npos ||
        lower.find("read-only variable") != std::string::npos ||
        lower.find("expression is not assignable") != std::string::npos) {
        return ErrorCode::CannotAssignToConstant;
    }

    if (lower.find("control reaches end of non-void function") != std::string::npos ||
        lower.find("non-void function does not return") != std::string::npos) {
        return ErrorCode::MissingReturnStatement;
    }

    if (lower.find("unreachable code") != std::string::npos ||
        lower.find("code will never be executed") != std::string::npos) {
        return ErrorCode::UnreachableCode;
    }

    if (lower.find("subscript") != std::string::npos ||
        lower.find("operator[]") != std::string::npos) {
        return ErrorCode::InvalidIndexOperation;
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
    driver::CompilerKind kind,
    const SourceManager* source_mgr,
    const SourceMap* source_map) {
    auto raw_diags = parse_compiler_output(raw_output, kind);
    std::vector<Diagnostic> result;
    result.reserve(raw_diags.size());

    for (const auto& raw : raw_diags) {
        ErrorCode code = map_to_error_code(raw.cleanMessage);
        std::string file = raw.file;
        uint32_t line = raw.line;
        uint32_t col = raw.column;
        std::optional<SourceSpan> resolved_span;

        // 1. If file is a generated C++ file or source_map is available:
        bool is_cpp_file = file.ends_with(".cpp") || file.ends_with(".cc") ||
                           file.ends_with(".cxx") || file.ends_with(".h") ||
                           file.ends_with(".hpp");

        if (source_map) {
            if (is_cpp_file) {
                auto mapped = source_map->findSourceLocation(line, col);
                if (mapped.has_value()) {
                    file = mapped->sourceFile;
                    line = mapped->sourceLine;
                    if (mapped->sourceColumn > 0) col = mapped->sourceColumn;
                    if (mapped->sourceSpan.isValid()) resolved_span = mapped->sourceSpan;
                }
            } else {
                auto mapped = source_map->findEntryForSourceLine(file, line);
                if (mapped.has_value()) {
                    if (mapped->sourceSpan.isValid()) resolved_span = mapped->sourceSpan;
                    if (col == 0 && mapped->sourceColumn > 0) col = mapped->sourceColumn;
                }
            }
        }

        // 2. Span reconstruction using SourceManager
        FileId fid = INVALID_FILE_ID;
        const SourceBuffer* buf = nullptr;
        if (source_mgr) {
            buf = source_mgr->getBufferByPath(file);
            if (!buf) {
                // Try fuzzy path matching (e.g. filename only or relative path)
                for (FileId id = 0; ; ++id) {
                    const auto* b = source_mgr->getBuffer(id);
                    if (!b) break;
                    if (b->getFilePath() == file ||
                        b->getFilePath().ends_with("/" + file) ||
                        file.ends_with("/" + b->getFilePath())) {
                        buf = b;
                        break;
                    }
                }
            }
        }

        if (buf) {
            fid = buf->getId();

            if (resolved_span.has_value() && resolved_span->isValid() && resolved_span->start.line == line) {
                resolved_span->start.fileId = fid;
                resolved_span->end.fileId = fid;
            } else {
                // Reconstruct span from buffer line text
                if (line > 0 && line <= buf->getLineCount()) {
                    std::string_view line_str = buf->getLineText(line);

                    size_t token_col = col;
                    size_t token_len = 1;

                    // Extract quoted identifier from raw.cleanMessage if any (e.g. 'hp')
                    size_t q1 = raw.cleanMessage.find('\'');
                    std::string quoted_sym;
                    if (q1 != std::string::npos) {
                        size_t q2 = raw.cleanMessage.find('\'', q1 + 1);
                        if (q2 != std::string::npos) {
                            quoted_sym = raw.cleanMessage.substr(q1 + 1, q2 - q1 - 1);
                        }
                    }

                    // If quoted symbol is on the source line, snap squiggle directly to it!
                    if (!quoted_sym.empty()) {
                        size_t sym_pos = line_str.find(quoted_sym);
                        if (sym_pos != std::string_view::npos) {
                            token_col = sym_pos + 1;
                            token_len = quoted_sym.size();
                            col = static_cast<uint32_t>(token_col);
                        }
                    }

                    if (token_col == 0) {
                        size_t first_non_ws = line_str.find_first_not_of(" \t");
                        if (first_non_ws != std::string_view::npos) {
                            token_col = first_non_ws + 1;
                        } else {
                            token_col = 1;
                        }
                        col = static_cast<uint32_t>(token_col);
                    }

                    if (token_len == 1 && token_col > 0 && token_col <= line_str.size()) {
                        size_t start_idx = token_col - 1;
                        size_t end_idx = start_idx;
                        if (isalnum(static_cast<unsigned char>(line_str[start_idx])) || line_str[start_idx] == '_') {
                            while (end_idx < line_str.size() &&
                                   (isalnum(static_cast<unsigned char>(line_str[end_idx])) || line_str[end_idx] == '_')) {
                                end_idx++;
                            }
                        } else {
                            end_idx = start_idx + 1;
                        }
                        token_len = std::max<size_t>(1, end_idx - start_idx);
                    }

                    auto start_off = buf->getOffsetAtLocation(line, static_cast<uint32_t>(token_col));
                    auto end_off = buf->getOffsetAtLocation(line, static_cast<uint32_t>(token_col + token_len));
                    if (start_off.has_value() && end_off.has_value()) {
                        resolved_span = SourceSpan{
                            SourceLocation{fid, *start_off, line, static_cast<uint32_t>(token_col)},
                            SourceLocation{fid, *end_off, line, static_cast<uint32_t>(token_col + token_len)}
                        };
                    }
                }
            }
        }

        SourceLocation loc{};
        loc.fileId = fid;
        loc.line = line;
        loc.column = col;
        if (resolved_span.has_value() && resolved_span->isValid()) {
            loc.offset = resolved_span->start.offset;
        }

        Diagnostic d = (raw.severity == DiagnosticSeverity::Warning)
            ? Diagnostic::makeWarning(std::string(getErrorCodeString(code)), raw.cleanMessage, loc)
            : Diagnostic::makeError(code, raw.cleanMessage, loc);

        if (resolved_span.has_value() && resolved_span->isValid()) {
            d.withLabel(*resolved_span, "");
        }

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

std::vector<LinkerSymbol> DiagnosticTranslator::parse_linker_output(std::string_view raw_output) {
    std::vector<LinkerSymbol> result;
    std::istringstream stream{std::string(raw_output)};
    std::string line;

    LinkerSymbol* current_symbol = nullptr;
    bool in_ld64_undefined_block = false;

    // Regex matchers
    static const std::regex gnu_undefined_re(
        R"((?:undefined reference to|undefined symbol:?)\s*['`"]?([^'`"]+)['`"]?)",
        std::regex::optimize);
    static const std::regex msvc_undefined_re(
        R"(error LNK20(?:19|01):\s*unresolved external symbol\s+([^\s]+)(?:\s+referenced in function\s+(.+))?)",
        std::regex::optimize);
    static const std::regex ld64_symbol_re(
        R"(^\s*["']?(_[^"',\s]+)["']?,\s*referenced from:)",
        std::regex::optimize);
    static const std::regex lib_not_found_re(
        R"((?:library not found for -l|cannot find -l|cannot open file ')([^'\s]+))",
        std::regex::optimize);

    while (std::getline(stream, line)) {
        std::string_view trimmed = trim_sv(line);
        if (trimmed.empty()) continue;

        // Check for missing library flags: -lfoo or foo.lib
        std::smatch lib_match;
        std::string line_str(trimmed);
        if (std::regex_search(line_str, lib_match, lib_not_found_re)) {
            std::string lib_name = lib_match[1].str();
            if (lib_name.ends_with(".lib")) {
                lib_name = lib_name.substr(0, lib_name.size() - 4);
            }
            if (lib_name.ends_with('\'')) {
                lib_name.pop_back();
            }

            LinkerSymbol sym;
            sym.rawSymbol = "-l" + lib_name;
            sym.demangledSymbol = lib_name;
            sym.suggestedPackage = lib_name;
            sym.referencedFrom = "linker library flag -l" + lib_name;
            result.push_back(std::move(sym));
            current_symbol = nullptr;
            continue;
        }

        // Check ld64 block header
        if (trimmed.rfind("Undefined symbols for architecture", 0) == 0 ||
            trimmed == "Undefined symbols:" ||
            trimmed.rfind("Undefined symbols:", 0) == 0) {
            in_ld64_undefined_block = true;
            continue;
        }

        // Inside ld64 block
        if (in_ld64_undefined_block) {
            if (trimmed.rfind("ld: symbol(s) not found", 0) == 0 ||
                trimmed.rfind("ld: fatal error", 0) == 0) {
                in_ld64_undefined_block = false;
                current_symbol = nullptr;
                continue;
            }

            std::smatch ld64_match;
            if (std::regex_search(line, ld64_match, ld64_symbol_re)) {
                std::string raw_sym = ld64_match[1].str();
                LinkerSymbol sym;
                sym.rawSymbol = raw_sym;
                sym.demangledSymbol = demangle_symbol(raw_sym);
                sym.suggestedPackage = suggest_package_for_symbol(sym.demangledSymbol);
                result.push_back(std::move(sym));
                current_symbol = &result.back();
                continue;
            }

            // Reference line inside ld64: indented with 6+ spaces
            if (current_symbol && (line.rfind("      ", 0) == 0 || line.rfind("\t\t", 0) == 0 || trimmed.rfind("in ", 0) != std::string_view::npos)) {
                if (current_symbol->referencedFrom.empty()) {
                    current_symbol->referencedFrom = std::string(trimmed);
                }
                continue;
            }
        }

        // Check MSVC unresolved external symbol
        std::smatch msvc_match;
        if (std::regex_search(line_str, msvc_match, msvc_undefined_re)) {
            std::string raw_sym = msvc_match[1].str();
            LinkerSymbol sym;
            sym.rawSymbol = raw_sym;
            sym.demangledSymbol = demangle_symbol(raw_sym);
            sym.suggestedPackage = suggest_package_for_symbol(sym.demangledSymbol);
            if (msvc_match.size() > 2 && msvc_match[2].matched) {
                sym.referencedFrom = msvc_match[2].str();
            }
            result.push_back(std::move(sym));
            current_symbol = &result.back();
            continue;
        }

        // Check GNU ld / gold undefined reference
        std::smatch gnu_match;
        if (std::regex_search(line_str, gnu_match, gnu_undefined_re)) {
            std::string raw_sym = gnu_match[1].str();
            LinkerSymbol sym;
            sym.rawSymbol = raw_sym;
            sym.demangledSymbol = demangle_symbol(raw_sym);
            sym.suggestedPackage = suggest_package_for_symbol(sym.demangledSymbol);

            size_t in_func_pos = line_str.find("in function");
            if (in_func_pos != std::string::npos) {
                size_t q1 = line_str.find_first_of("'`\"", in_func_pos);
                if (q1 != std::string::npos) {
                    size_t q2 = line_str.find_first_of("'`\"", q1 + 1);
                    if (q2 != std::string::npos) {
                        sym.referencedFrom = demangle_symbol(line_str.substr(q1 + 1, q2 - q1 - 1));
                    }
                }
            } else {
                size_t colon_pos = line_str.find(':');
                if (colon_pos != std::string::npos && colon_pos < 50) {
                    sym.referencedFrom = std::string(trim_sv(line_str.substr(0, colon_pos)));
                }
            }

            result.push_back(std::move(sym));
            current_symbol = &result.back();
            continue;
        }
    }

    return result;
}

std::vector<Diagnostic> DiagnosticTranslator::translate_linker_output(std::string_view raw_output) {
    auto symbols = parse_linker_output(raw_output);
    std::vector<Diagnostic> result;
    result.reserve(symbols.size());

    for (const auto& sym : symbols) {
        Diagnostic d;
        d.severity = DiagnosticSeverity::Error;
        d.code = std::string(getErrorCodeString(ErrorCode::ExternSymbolNotFound));

        if (sym.rawSymbol.rfind("-l", 0) == 0) {
            d.message = "native library '" + sym.demangledSymbol + "' was not found by the linker";
            d.withHelp("did you forget to install library '" + sym.demangledSymbol + "' or configure native library paths in keet.toml?");
        } else {
            d.message = "unresolved external symbol '" + sym.demangledSymbol + "'";

            if (!sym.referencedFrom.empty()) {
                d.withNote("referenced from " + sym.referencedFrom);
            }

            if (!sym.suggestedPackage.empty()) {
                d.withHelp("did you forget to declare package dependency '" + sym.suggestedPackage + "' in keet.toml?");
            } else {
                d.withHelp("ensure the native library containing '" + sym.demangledSymbol + "' is linked or installed on your system");
            }
        }

        result.push_back(std::move(d));
    }

    return result;
}

} // namespace keety::diag
