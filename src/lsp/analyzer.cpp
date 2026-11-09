#include <keety/lsp/analyzer.hpp>

#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/ast/type.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/driver/project_root.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/kml/kml_parser.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/resolver/module_path_resolver.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/universe.hpp>
#include <keety/fmt/formatter.hpp>
#include <keety/fmt/formatter_options.hpp>
#include <keety/lint/lint_config.hpp>
#include <keety/lint/lint_engine.hpp>
#include <keety/typechecker/type_checker.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace keety::lsp {

namespace {

std::string uriToPath(const std::string& uri) {
    if (uri.rfind("file://", 0) == 0) {
        std::string path = uri.substr(7);
#if defined(_WIN32)
        if (path.size() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
            path = path.substr(1); // /C:/... -> C:/...
        }
#endif
        return path;
    }
    return uri;
}

Range spanToRange(const Document& doc, const diag::SourceSpan& span) {
    if (!span.isValid()) {
        return Range{Position{0, 0}, Position{0, 0}};
    }

    Position startPos = doc.offsetToPosition(span.start.offset);
    Position endPos = doc.offsetToPosition(span.end.offset);

    if (endPos.line < startPos.line || (endPos.line == startPos.line && endPos.character < startPos.character)) {
        endPos = startPos;
    }
    return Range{startPos, endPos};
}

Range findIdentifierRange(const Document& doc, const std::string& name, const diag::SourceSpan& enclosingSpan) {
    if (!enclosingSpan.isValid() || name.empty()) {
        return spanToRange(doc, enclosingSpan);
    }
    const std::string& text = doc.text();
    size_t start = enclosingSpan.start.offset;
    size_t end = std::min(static_cast<size_t>(enclosingSpan.end.offset), text.size());
    if (start < end) {
        size_t found = text.find(name, start);
        if (found != std::string::npos && found + name.size() <= end) {
            Position s = doc.offsetToPosition(found);
            Position e = doc.offsetToPosition(found + name.size());
            return Range{s, e};
        }
    }
    return spanToRange(doc, enclosingSpan);
}

DiagnosticSeverity convertSeverity(diag::DiagnosticSeverity sev) {
    switch (sev) {
    case diag::DiagnosticSeverity::Error:
        return DiagnosticSeverity::Error;
    case diag::DiagnosticSeverity::Warning:
        return DiagnosticSeverity::Warning;
    case diag::DiagnosticSeverity::Note:
        return DiagnosticSeverity::Information;
    case diag::DiagnosticSeverity::Help:
        return DiagnosticSeverity::Hint;
    }
    return DiagnosticSeverity::Error;
}

void collectLspDiagnostics(
    const Document& doc,
    const std::vector<diag::Diagnostic>& compilerDiags,
    std::vector<Diagnostic>& outLspDiags
) {
    for (const auto& d : compilerDiags) {
        Diagnostic lspDiag;
        lspDiag.severity = convertSeverity(d.severity);
        lspDiag.code = d.code;
        lspDiag.message = d.message;

        Range diagRange{Position{0, 0}, Position{0, 0}};
        bool foundPrimary = false;

        for (const auto& label : d.labels) {
            if (label.isPrimary && label.span.isValid()) {
                diagRange = spanToRange(doc, label.span);
                foundPrimary = true;
                break;
            }
        }

        if (!foundPrimary) {
            if (d.location.isValid()) {
                Position p = doc.offsetToPosition(d.location.offset);
                diagRange = Range{p, Position{p.line, p.character + 1}};
            }
        }

        for (const auto& f : d.footers) {
            lspDiag.message += "\n" + f.message;
        }

        for (const auto& label : d.labels) {
            if (!label.isPrimary && label.span.isValid()) {
                DiagnosticRelatedInformation rel;
                rel.uri = doc.uri();
                rel.range = spanToRange(doc, label.span);
                rel.message = label.message;
                lspDiag.relatedInformation.push_back(std::move(rel));
            }
        }

        lspDiag.range = diagRange;
        outLspDiags.push_back(std::move(lspDiag));
    }
}

std::string formatType(const ast::TypeAnnotation* annot, const std::string& fallback = "void") {
    if (!annot) return fallback;
    auto t = typechecker::Type::fromAnnotation(annot);
    if (t) return t->toString();
    return fallback;
}

std::string extractDocComments(const std::string& source, size_t declStartOffset) {
    if (declStartOffset == 0 || declStartOffset > source.size()) {
        return "";
    }
    size_t pos = declStartOffset;
    while (pos > 0 && (source[pos - 1] == ' ' || source[pos - 1] == '\t' || source[pos - 1] == '\r' || source[pos - 1] == '\n')) {
        pos--;
    }

    // If preceded by modifiers like "export", "pub", or "priv", skip over them to find doc comments
    if (pos >= 6 && source.substr(pos - 6, 6) == "export") {
        pos -= 6;
        while (pos > 0 && (source[pos - 1] == ' ' || source[pos - 1] == '\t' || source[pos - 1] == '\r' || source[pos - 1] == '\n')) {
            pos--;
        }
    } else if (pos >= 3 && source.substr(pos - 3, 3) == "pub") {
        pos -= 3;
        while (pos > 0 && (source[pos - 1] == ' ' || source[pos - 1] == '\t' || source[pos - 1] == '\r' || source[pos - 1] == '\n')) {
            pos--;
        }
    } else if (pos >= 4 && source.substr(pos - 4, 4) == "priv") {
        pos -= 4;
        while (pos > 0 && (source[pos - 1] == ' ' || source[pos - 1] == '\t' || source[pos - 1] == '\r' || source[pos - 1] == '\n')) {
            pos--;
        }
    }

    std::vector<std::string> lines;
    while (pos > 0) {
        size_t lineEnd = pos;
        size_t lineStart = lineEnd;
        while (lineStart > 0 && source[lineStart - 1] != '\n') {
            lineStart--;
        }
        std::string line = source.substr(lineStart, lineEnd - lineStart + 1);
        size_t nonSpace = line.find_first_not_of(" \t\r\n");
        if (nonSpace != std::string::npos && line.compare(nonSpace, 3, "///") == 0) {
            std::string content = line.substr(nonSpace + 3);
            if (!content.empty() && content.front() == ' ') content = content.substr(1);
            while (!content.empty() && (content.back() == '\r' || content.back() == '\n' || content.back() == ' ')) {
                content.pop_back();
            }
            lines.push_back(content);
            if (lineStart == 0) break;
            pos = lineStart - 1;
        } else if (nonSpace != std::string::npos && line.compare(nonSpace, 2, "//") == 0) {
            std::string content = line.substr(nonSpace + 2);
            if (!content.empty() && content.front() == ' ') content = content.substr(1);
            while (!content.empty() && (content.back() == '\r' || content.back() == '\n' || content.back() == ' ')) {
                content.pop_back();
            }
            lines.push_back(content);
            if (lineStart == 0) break;
            pos = lineStart - 1;
        } else {
            break;
        }
    }

    if (lines.empty()) return "";
    std::reverse(lines.begin(), lines.end());
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += "\n";
        result += lines[i];
    }
    return result;
}

struct StdlibDoc {
    std::string signature;
    std::string doc;
};

std::optional<StdlibDoc> getStdlibMemberDoc(const std::string& name) {
    static const std::unordered_map<std::string, StdlibDoc> docs = {
        {"println", {"fn println(...args: string | int | float | bool | char | null, color?: io.Color): void", "Prints arguments to standard output followed by a newline."}},
        {"print", {"fn print(...args: string | int | float | bool | char | null, color?: io.Color): void", "Prints arguments to standard output without a trailing newline."}},
        {"eprintln", {"fn eprintln(...args: string | int | float | bool | char | null, color?: io.Color): void", "Prints arguments to standard error followed by a newline."}},
        {"eprint", {"fn eprint(...args: string | int | float | bool | char | null, color?: io.Color): void", "Prints arguments to standard error without a trailing newline."}},
        {"readln", {"fn readln(): string", "Reads a line of text from standard input (without newline)."}},
        {"readLine", {"fn readLine(): string | null", "Reads a line of text from standard input (without newline), or null on EOF."}},
        {"input", {"fn input(prompt?: string): string", "Displays an optional prompt and reads a line of user input."}},
        {"eof", {"fn eof(): bool", "Returns true if standard input has reached end-of-file (EOF)."}},
        {"flush", {"fn flush(): void", "Flushes standard output buffer."}},
        {"red", {"fn red(s: string): string", "Formats string with ANSI red color."}},
        {"green", {"fn green(s: string): string", "Formats string with ANSI green color."}},
        {"yellow", {"fn yellow(s: string): string", "Formats string with ANSI yellow color."}},
        {"blue", {"fn blue(s: string): string", "Formats string with ANSI blue color."}},
        {"magenta", {"fn magenta(s: string): string", "Formats string with ANSI magenta color."}},
        {"cyan", {"fn cyan(s: string): string", "Formats string with ANSI cyan color."}},
        {"white", {"fn white(s: string): string", "Formats string with ANSI white color."}},
        {"gray", {"fn gray(s: string): string", "Formats string with ANSI gray color."}},
        {"bold", {"fn bold(s: string): string", "Formats string with ANSI bold style."}},
        {"Color", {"enum io.Color", "Terminal output color enum (Default, Red, Green, Yellow, Blue, Magenta, Cyan, White, Gray)."}},
        {"readFile", {"fn readFile(path: string): string", "Reads the entire contents of a file as a string."}},
        {"writeFile", {"fn writeFile(path: string, content: string): void", "Writes string content to a file."}},
        {"appendFile", {"fn appendFile(path: string, content: string): void", "Appends string content to a file."}},
        {"exists", {"fn exists(path: string): bool", "Returns true if the given path exists."}},
        {"removeFile", {"fn removeFile(path: string): void", "Deletes a file at the specified path."}},
        {"mkdir", {"fn mkdir(path: string): void", "Creates a directory at the specified path."}},
        {"readDir", {"fn readDir(path: string): array<string>", "Returns a list of entry names in the directory."}},
        {"exit", {"fn exit(code: int): void", "Terminates the process with the given exit code."}},
        {"args", {"fn args(): array<string>", "Returns the command-line arguments passed to the program."}},
        {"getEnv", {"fn getEnv(key: string): string | null", "Retrieves the value of an environment variable."}},
        {"setEnv", {"fn setEnv(key: string, value: string): void", "Sets an environment variable."}},
        {"platform", {"fn platform(): string", "Returns the operating system platform ('macos', 'linux', 'windows')."}},
        {"PI", {"const PI: float", "Ratio of the circumference of a circle to its diameter (~3.14159)."}},
        {"E", {"const E: float", "Euler's number and the base of natural logarithms (~2.71828)."}},
        {"sqrt", {"fn sqrt(x: float): float", "Returns the square root of x."}},
        {"pow", {"fn pow(base: float, exp: float): float", "Returns base raised to the power of exp."}},
        {"abs", {"fn abs(x: float): float", "Returns the absolute value of x."}},
        {"sin", {"fn sin(x: float): float", "Returns the sine of x (in radians)."}},
        {"cos", {"fn cos(x: float): float", "Returns the cosine of x (in radians)."}},
        {"floor", {"fn floor(x: float): float", "Returns the largest integer less than or equal to x."}},
        {"ceil", {"fn ceil(x: float): float", "Returns the smallest integer greater than or equal to x."}},
        {"round", {"fn round(x: float): float", "Rounds x to the nearest integer."}},
        {"min", {"fn min(a: float, b: float): float", "Returns the smaller of two numbers."}},
        {"max", {"fn max(a: float, b: float): float", "Returns the larger of two numbers."}},
        {"now", {"fn now(): int", "Returns the current Unix timestamp in milliseconds."}},
        {"sleep", {"fn sleep(ms: int): void", "Suspends execution for the specified number of milliseconds."}},
        {"parse", {"fn parse(text: string): any", "Parses a JSON string into KeetScript data structures."}},
        {"stringify", {"fn stringify(value: any): string", "Serializes a KeetScript value into a JSON string."}},
        {"fetch", {"fn fetch(url: string): string", "Performs an HTTP GET request and returns the response body."}},
        {"case", {"fn case(name: string, fn: (): void): void", "Registers a test case with the test runner."}},
        {"suite", {"fn suite(name: string, fn: (): void): void", "Groups related test cases into a suite."}},
        {"assertEq", {"fn assertEq(actual: any, expected: any, msg?: string): void", "Asserts that two values are deeply equal."}},
        {"assertNe", {"fn assertNe(actual: any, expected: any, msg?: string): void", "Asserts that two values are not equal."}},
        {"assertTrue", {"fn assertTrue(condition: bool, msg?: string): void", "Asserts that a boolean condition is true."}},
        {"assertFalse", {"fn assertFalse(condition: bool, msg?: string): void", "Asserts that a boolean condition is false."}},
        {"assertZonks", {"fn assertZonks(fn: (): void, expectedError?: string, msg?: string): void", "Asserts that a callback throws a KeetScript zonk error."}},
        {"fail", {"fn fail(msg?: string): void", "Explicitly fails the current test case."}},
        // Standard Library Module: crypto
        {"toHex", {"fn toHex(data: string): string", "Encodes binary data or string into lowercase hexadecimal representation."}},
        {"fromHex", {"fn fromHex(hexStr: string): string", "Decodes a hexadecimal string into binary data or ASCII string."}},
        {"toBase64", {"fn toBase64(data: string): string", "Encodes data into standard RFC 4648 Base64 representation."}},
        {"fromBase64", {"fn fromBase64(b64Str: string): string", "Decodes standard RFC 4648 Base64 encoded string."}},
        {"toBase64Url", {"fn toBase64Url(data: string): string", "Encodes data into URL-safe Base64 representation without padding."}},
        {"fromBase64Url", {"fn fromBase64Url(b64UrlStr: string): string", "Decodes URL-safe Base64 encoded string."}},
        {"randomBytes", {"fn randomBytes(size: int): string", "Generates cryptographically secure random bytes of the specified length."}},
        {"randomHex", {"fn randomHex(size: int): string", "Generates cryptographically secure random hex string of the specified byte count."}},
        {"randomInt", {"fn randomInt(min: int, max: int): int", "Generates a uniform cryptographically secure random integer in [min, max]."}},
        {"randomUuid", {"fn randomUuid(): string", "Generates an RFC 4122 Version 4 random UUID string."}},
        {"timingSafeEqual", {"fn timingSafeEqual(a: string, b: string): bool", "Constant-time string/byte buffer comparison to mitigate side-channel timing attacks."}},
        {"sha256", {"fn sha256(data: string): string", "Computes SHA-256 hash returned as lowercase hex string."}},
        {"sha512", {"fn sha512(data: string): string", "Computes SHA-512 hash returned as lowercase hex string."}},
        {"sha3_256", {"fn sha3_256(data: string): string", "Computes SHA3-256 (FIPS 202) hash returned as lowercase hex string."}},
        {"sha3_512", {"fn sha3_512(data: string): string", "Computes SHA3-512 (FIPS 202) hash returned as lowercase hex string."}},
        {"blake3", {"fn blake3(data: string): string", "Computes BLAKE3 high-performance cryptographic hash returned as hex string."}},
        {"createHash", {"fn createHash(algorithm: string): Hasher", "Creates an incremental streaming cryptographic hash instance ('sha256', 'sha512', 'sha3-256', 'sha3-512', 'blake3')."}},
        {"hmacSha256", {"fn hmacSha256(key: string, data: string): string", "Computes HMAC-SHA256 message authentication code returned as hex string."}},
        {"hmacSha512", {"fn hmacSha512(key: string, data: string): string", "Computes HMAC-SHA512 message authentication code returned as hex string."}},
        {"createHmac", {"fn createHmac(algorithm: string, key: string): Hmac", "Creates an incremental streaming HMAC instance ('sha256' or 'sha512')."}},
        {"encryptChaCha20Poly1305", {"fn encryptChaCha20Poly1305(plaintext: string, key: string, iv: string, aad?: string): EncryptedPayload", "Encrypts data using ChaCha20-Poly1305 AEAD (RFC 8439)."}},
        {"decryptChaCha20Poly1305", {"fn decryptChaCha20Poly1305(payload: EncryptedPayload, key: string, aad?: string): string", "Decrypts and verifies data using ChaCha20-Poly1305 AEAD (RFC 8439)."}},
        {"encryptAesGcm", {"fn encryptAesGcm(plaintext: string, key: string, iv: string, aad?: string): EncryptedPayload", "Encrypts data using AES-256-GCM AEAD (NIST SP 800-38D)."}},
        {"decryptAesGcm", {"fn decryptAesGcm(payload: EncryptedPayload, key: string, aad?: string): string", "Decrypts and verifies data using AES-256-GCM AEAD (NIST SP 800-38D)."}},
        {"encodeCode128", {"fn encodeCode128(text: string): string", "Encodes text into a Code 128 barcode binary pattern ('1' for bar, '0' for space)."}},
        {"encodeCode128Svg", {"fn encodeCode128Svg(text: string, height?: int): string", "Renders Code 128 barcode as a standalone vector SVG XML string."}},
        {"encodeCode128Ascii", {"fn encodeCode128Ascii(text: string): string", "Renders Code 128 barcode as an ASCII art block string."}},
        {"decodeCode128", {"fn decodeCode128(pattern: string): string", "Decodes a Code 128 binary pattern string ('1' and '0') back into text."}},
        {"generateQr", {"fn generateQr(text: string, eccLevel?: string): QrCode", "Generates a 2D QR Code ISO/IEC 18004 matrix with specified ECC level ('L', 'M', 'Q', 'H')."}},
        {"decodeQr", {"fn decodeQr(matrix: array<array<bool>>): string", "Decodes a 2D boolean module matrix of a QR Code into string text."}},
        {"EncryptedPayload", {"class EncryptedPayload", "AEAD encryption payload containing ciphertext, auth tag, and initialization vector (iv)."}},
        {"QrCode", {"class QrCode", "QR Code model providing SVG/ASCII rendering and matrix access."}},
        {"Hasher", {"class Hasher", "Streaming cryptographic hash accumulator (`update(data)`, `digestHex()`)."}},
        {"Hmac", {"class Hmac", "Streaming HMAC accumulator (`update(data)`, `digestHex()`)."}},
    };
    auto it = docs.find(name);
    if (it != docs.end()) return it->second;
    return std::nullopt;
}

std::optional<std::string> getKeywordDoc(lexer::TokenKind kind) {
    using namespace lexer;
    switch (kind) {
    case TokenKind::KwFn:
        return "```keet\nfn <name>(<params>): <ReturnType> { ... }\n```\nDeclares a named function with explicit parameter and return types.";
    case TokenKind::KwLet:
        return "```keet\nlet <name>[: <Type>] = <expr>;\n```\nDeclares an immutable local or module variable binding.";
    case TokenKind::KwMut:
        return "```keet\nmut <name>[: <Type>] = <expr>;\n```\nDeclares a mutable variable binding.";
    case TokenKind::KwConst:
        return "```keet\nconst <NAME>[: <Type>] = <expr>;\n```\nDeclares a compile-time constant.";
    case TokenKind::KwDefine:
        return "```keet\ndefine <NAME> = <expr>;\n```\nMacro define directive.";
    case TokenKind::KwClass:
        return "```keet\nclass <Name> [inherits <Base>] { ... }\n```\nDeclares an object-oriented class with fields and methods.";
    case TokenKind::KwInherits:
        return "```keet\nclass Child inherits Parent\n```\nSpecifies the base class for single inheritance.";
    case TokenKind::KwInherited:
        return "```keet\ninherited.method()\n```\nAccesses the superclass implementation of a method or constructor.";
    case TokenKind::KwThis:
        return "```keet\nthis.field\n```\nRefers to the current class instance.";
    case TokenKind::KwEnum:
        return "```keet\nenum <Name> { ... }\n```\nDeclares an enumerated type.";
    case TokenKind::KwImport:
        return "```keet\nimport <module>;\nimport \"<relative_path>\";\nfrom \"<path>\" import <sym>;\n```\nImports modules, local files, or third-party packages.";
    case TokenKind::KwExport:
    case TokenKind::KwPub:
        return "```keet\npub <declaration>\n```\nExports a symbol making it accessible to other modules.";
    case TokenKind::KwPriv:
        return "```keet\npriv <declaration>\n```\nRestricts visibility to the declaring class or module.";
    case TokenKind::KwIf:
    case TokenKind::KwElse:
    case TokenKind::KwElif:
        return "Conditional branch control flow keyword.";
    case TokenKind::KwFor:
    case TokenKind::KwIn:
        return "```keet\nfor item in collection { ... }\n```\nIteration loop over arrays, strings, or maps.";
    case TokenKind::KwWhile:
        return "```keet\nwhile condition { ... }\n```\nLoop executed while a condition evaluates to true.";
    case TokenKind::KwReturn:
        return "```keet\nreturn [expr];\n```\nReturns control and an optional value from a function.";
    case TokenKind::KwYield:
        return "```keet\nyield [expr];\n```\nYields a value from a generator or coroutine.";
    case TokenKind::KwBreak:
        return "Terminates the nearest enclosing loop or switch statement.";
    case TokenKind::KwContinue:
        return "Skips to the next iteration of the enclosing loop.";
    case TokenKind::KwSwitch:
    case TokenKind::KwCase:
    case TokenKind::KwDefault:
        return "Pattern matching and multi-way branch selection.";
    case TokenKind::KwTypeof:
        return "```keet\ntypeof <expr>\n```\nEvaluates the type descriptor of an expression at runtime.";
    case TokenKind::KwAs:
        return "```keet\n<expr> as <Type>\n```\nType cast / assertion expression.";
    case TokenKind::KwFrom:
        return "```keet\nfrom \"<path>\" import <sym1>, <sym2>;\n```\nSelective symbol import from a module.";
    case TokenKind::KwErrtype:
        return "```keet\nerrtype <Name> { ... }\n```\nDeclares a typed error domain for structured error recovery.";
    case TokenKind::KwZonk:
        return "```keet\nzonk <error>;\n```\nRaises an unrecoverable runtime exception / panic.";
    case TokenKind::KwInt:
        return "```keet\ntype int\n```\n64-bit signed integer (`int64_t`).";
    case TokenKind::KwFloat:
        return "```keet\ntype float\n```\n64-bit IEEE 754 double precision floating point (`double`).";
    case TokenKind::KwBool:
        return "```keet\ntype bool\n```\nBoolean type (`true` or `false`).";
    case TokenKind::KwChar:
        return "```keet\ntype char\n```\n32-bit Unicode character (code point).";
    case TokenKind::KwString:
        return "```keet\ntype string\n```\nImmutable UTF-8 string with length and slice operations.";
    case TokenKind::KwVoid:
        return "```keet\ntype void\n```\nRepresents the absence of a return value.";
    case TokenKind::KwError:
        return "```keet\ntype error\n```\nBuilt-in runtime error type.";
    case TokenKind::KwArray:
        return "```keet\ntype array<T>\n```\nDynamically-sized growable array of type `T`.";
    case TokenKind::KwMap:
        return "```keet\ntype map<K, V>\n```\nAssociative key-value hash map.";
    case TokenKind::KwTrue:
        return "Boolean literal `true`.";
    case TokenKind::KwFalse:
        return "Boolean literal `false`.";
    case TokenKind::KwNull:
        return "Null literal `null` representing the absence of an object or reference.";
    default:
        return std::nullopt;
    }
}

lexer::TokenKind kwToTokenKind(const std::string& kw) {
    using namespace lexer;
    if (kw == "fn") return TokenKind::KwFn;
    if (kw == "let") return TokenKind::KwLet;
    if (kw == "mut") return TokenKind::KwMut;
    if (kw == "const") return TokenKind::KwConst;
    if (kw == "define") return TokenKind::KwDefine;
    if (kw == "class") return TokenKind::KwClass;
    if (kw == "inherits") return TokenKind::KwInherits;
    if (kw == "inherited") return TokenKind::KwInherited;
    if (kw == "this") return TokenKind::KwThis;
    if (kw == "enum") return TokenKind::KwEnum;
    if (kw == "import") return TokenKind::KwImport;
    if (kw == "export") return TokenKind::KwExport;
    if (kw == "pub") return TokenKind::KwPub;
    if (kw == "priv") return TokenKind::KwPriv;
    if (kw == "if") return TokenKind::KwIf;
    if (kw == "else") return TokenKind::KwElse;
    if (kw == "elif") return TokenKind::KwElif;
    if (kw == "for") return TokenKind::KwFor;
    if (kw == "in") return TokenKind::KwIn;
    if (kw == "while") return TokenKind::KwWhile;
    if (kw == "return") return TokenKind::KwReturn;
    if (kw == "yield") return TokenKind::KwYield;
    if (kw == "break") return TokenKind::KwBreak;
    if (kw == "continue") return TokenKind::KwContinue;
    if (kw == "switch") return TokenKind::KwSwitch;
    if (kw == "case") return TokenKind::KwCase;
    if (kw == "default") return TokenKind::KwDefault;
    if (kw == "typeof") return TokenKind::KwTypeof;
    if (kw == "as") return TokenKind::KwAs;
    if (kw == "from") return TokenKind::KwFrom;
    if (kw == "errtype") return TokenKind::KwErrtype;
    if (kw == "zonk") return TokenKind::KwZonk;
    return TokenKind::Invalid;
}

void collectLocalBindings(
    const ast::Stmt* stmt,
    size_t targetOffset,
    std::vector<CompletionItem>& items,
    std::unordered_set<std::string>& seen
) {
    if (!stmt) return;
    if (const auto* block = dynamic_cast<const ast::BlockStmt*>(stmt)) {
        for (const auto& s : block->getStatements()) {
            if (!s) continue;
            if (s->getSpan().start.offset >= targetOffset) {
                continue;
            }
            if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(s.get())) {
                const auto* d = declStmt->getDeclaration();
                if (const auto* vd = dynamic_cast<const ast::VarDecl*>(d)) {
                    if (seen.insert(vd->getName()).second) {
                        CompletionItem item;
                        item.label = vd->getName();
                        item.kind = CompletionItemKind::Variable;
                        item.detail = "let " + vd->getName() + ": " + formatType(vd->getType(), "any");
                        items.push_back(std::move(item));
                    }
                } else if (const auto* cd = dynamic_cast<const ast::ConstDecl*>(d)) {
                    if (seen.insert(cd->getName()).second) {
                        CompletionItem item;
                        item.label = cd->getName();
                        item.kind = CompletionItemKind::Constant;
                        item.detail = "const " + cd->getName() + ": " + formatType(cd->getType(), "any");
                        items.push_back(std::move(item));
                    }
                }
            } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(s.get())) {
                collectLocalBindings(ifStmt->getThenBranch(), targetOffset, items, seen);
                collectLocalBindings(ifStmt->getElseBranch(), targetOffset, items, seen);
            } else if (const auto* forInStmt = dynamic_cast<const ast::ForInStmt*>(s.get())) {
                if (targetOffset >= forInStmt->getSpan().start.offset && targetOffset <= forInStmt->getSpan().end.offset) {
                    if (!forInStmt->getValueName().empty() && seen.insert(forInStmt->getValueName()).second) {
                        CompletionItem item;
                        item.label = forInStmt->getValueName();
                        item.kind = CompletionItemKind::Variable;
                        item.detail = "(loop item) " + forInStmt->getValueName() + ": " + formatType(forInStmt->getValueType(), "any");
                        items.push_back(std::move(item));
                    }
                    if (!forInStmt->getKeyName().empty() && seen.insert(forInStmt->getKeyName()).second) {
                        CompletionItem item;
                        item.label = forInStmt->getKeyName();
                        item.kind = CompletionItemKind::Variable;
                        item.detail = "(loop key) " + forInStmt->getKeyName() + ": " + formatType(forInStmt->getKeyType(), "any");
                        items.push_back(std::move(item));
                    }
                    collectLocalBindings(forInStmt->getBody(), targetOffset, items, seen);
                }
            } else if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(s.get())) {
                if (targetOffset >= forStmt->getSpan().start.offset && targetOffset <= forStmt->getSpan().end.offset) {
                    collectLocalBindings(forStmt->getInit(), targetOffset, items, seen);
                    collectLocalBindings(forStmt->getBody(), targetOffset, items, seen);
                }
            } else if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(s.get())) {
                if (targetOffset >= whileStmt->getSpan().start.offset && targetOffset <= whileStmt->getSpan().end.offset) {
                    collectLocalBindings(whileStmt->getBody(), targetOffset, items, seen);
                }
            }
        }
    }
}

std::string findReceiverClassName(
    const parser::ModuleAST& ast,
    const std::string& varName,
    size_t targetOffset
) {
    // 1. Check enclosing function/method params
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (targetOffset >= fnDecl->getSpan().start.offset && targetOffset <= fnDecl->getSpan().end.offset) {
                for (const auto& p : fnDecl->getParams()) {
                    if (p.name == varName && p.type) {
                        if (const auto* ct = dynamic_cast<const ast::CustomType*>(p.type.get())) {
                            return ct->getName();
                        }
                    }
                }
            }
        } else if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& m : cd->getMethods()) {
                if (m && targetOffset >= m->getSpan().start.offset && targetOffset <= m->getSpan().end.offset) {
                    for (const auto& p : m->getParams()) {
                        if (p.name == varName && p.type) {
                            if (const auto* ct = dynamic_cast<const ast::CustomType*>(p.type.get())) {
                                return ct->getName();
                            }
                        }
                    }
                }
            }
        }
    }

    // 2. Search statements for variable declaration
    std::function<std::string(const ast::Stmt*)> searchStmt = [&](const ast::Stmt* s) -> std::string {
        if (!s) return "";
        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(s)) {
            for (const auto& stmt : block->getStatements()) {
                if (!stmt) continue;
                if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt.get())) {
                    if (const auto* vd = dynamic_cast<const ast::VarDecl*>(declStmt->getDeclaration())) {
                        if (vd->getName() == varName) {
                            if (vd->getType()) {
                                if (const auto* ct = dynamic_cast<const ast::CustomType*>(vd->getType())) {
                                    return ct->getName();
                                }
                            }
                            if (vd->getInitializer()) {
                                if (const auto* call = dynamic_cast<const ast::CallExpr*>(vd->getInitializer())) {
                                    if (const auto* id = dynamic_cast<const ast::IdentExpr*>(call->getCallee())) {
                                        return id->getName();
                                    }
                                }
                            }
                        }
                    }
                } else if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt.get())) {
                    auto r = searchStmt(ifStmt->getThenBranch());
                    if (!r.empty()) return r;
                    r = searchStmt(ifStmt->getElseBranch());
                    if (!r.empty()) return r;
                }
            }
        }
        return "";
    };

    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (targetOffset >= fnDecl->getSpan().start.offset && targetOffset <= fnDecl->getSpan().end.offset) {
                auto r = searchStmt(fnDecl->getBody());
                if (!r.empty()) return r;
            }
        } else if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& m : cd->getMethods()) {
                if (m && targetOffset >= m->getSpan().start.offset && targetOffset <= m->getSpan().end.offset) {
                    auto r = searchStmt(m->getBody());
                    if (!r.empty()) return r;
                }
            }
        }
    }

    // 3. Top-level variables
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* vd = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            if (vd->getName() == varName) {
                if (vd->getType()) {
                    if (const auto* ct = dynamic_cast<const ast::CustomType*>(vd->getType())) {
                        return ct->getName();
                    }
                }
                if (vd->getInitializer()) {
                    if (const auto* call = dynamic_cast<const ast::CallExpr*>(vd->getInitializer())) {
                        if (const auto* id = dynamic_cast<const ast::IdentExpr*>(call->getCallee())) {
                            return id->getName();
                        }
                    }
                }
            }
        }
    }

    return "";
}

} // namespace

std::vector<Diagnostic> Analyzer::analyze(const Document& doc) {
    std::vector<Diagnostic> lspDiagnostics;

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);

    std::string pathStr = uriToPath(doc.uri());
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());

    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();

    if (fsPath.extension() == ".kml" || doc.languageId() == "kml") {
        kml::KmlParser kmlParser(doc.text(), fileId, &sm, &emitter);
        (void)kmlParser.parseDocument(moduleName);

        if (emitter.hasErrors()) {
            collectLspDiagnostics(doc, emitter.getDiagnostics(), lspDiagnostics);
            return lspDiagnostics;
        }

        auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
        sema::registerUniverseSymbols(*universe);
        parser::ModuleAST emptyAst;
        emptyAst.fileId = fileId;
        emptyAst.moduleName = moduleName;
        std::vector<lexer::Comment> comments;

        lint::LintConfig lintConfig = lint::LintConfig::discover(fsPath.parent_path());
        lint::LintEngine lintEngine(lintConfig);
        lintEngine.registerDefaultRules();

        auto lintDiags = lintEngine.lintModule(emptyAst, comments, sm, doc.text(), pathStr, universe.get());
        for (const auto& ld : lintDiags) {
            Diagnostic lspDiag;
            lspDiag.severity = convertSeverity(ld.severity);
            lspDiag.code = ld.ruleCode;
            lspDiag.source = "keetlint";
            lspDiag.message = ld.message;
            lspDiag.range = spanToRange(doc, ld.span);
            for (const auto& f : ld.footers) {
                lspDiag.message += "\n" + f.message;
            }
            lspDiagnostics.push_back(std::move(lspDiag));
        }
        return lspDiagnostics;
    }

    // 1. Lexical & Syntactic Analysis (Parser)
    lexer::Lexer lexer(fileId, doc.text());
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer, &emitter);

    auto ast = parser.parseModule(moduleName);

    if (emitter.hasErrors()) {
        collectLspDiagnostics(doc, emitter.getDiagnostics(), lspDiagnostics);
        return lspDiagnostics;
    }

    // 2. Resolve Module Dependencies & Pre-populate Scopes
    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    auto moduleScope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);


    // Validate module imports against project root
    std::filesystem::path projectRoot = driver::findProjectRoot(fsPath);
    resolver::ModulePathResolver pathResolver(projectRoot, &emitter);

    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            const std::string& importPath = imp->getModulePath();
            auto resolved = pathResolver.resolve(fsPath, importPath, imp->getSpan());
            if (resolved.has_value()) {
                std::string effName = imp->getModuleAlias();
                if (effName.empty()) {
                    effName = std::filesystem::path(resolved->canonicalPath).stem().string();
                }
                if (!effName.empty()) {
                    if (!resolved->isStandardLibrary() &&
                        resolved->canonicalPath.extension() == ".kml" &&
                        imp->getModuleAlias().empty()) {
                        auto compSym = std::make_shared<sema::Symbol>(
                            effName, sema::SymbolKind::Class, imp->getSpan());
                        compSym->setIsType(true);
                        compSym->setExported(true);
                        moduleScope->define(compSym, false);
                    } else {
                        moduleScope->define(
                            std::make_shared<sema::Symbol>(effName, sema::SymbolKind::Module, imp->getSpan()),
                            false
                        );
                    }
                }
            }
        }
    }

    // 3. Type Checking (Explicit annotations, Zero-coercion, Contextual types, CFA narrowing)
    typechecker::TypeChecker tc(&emitter);
    tc.checkTypes(ast, moduleScope.get());

    // 4. AstLowering (Borrow checking, liveness, dead code verification)
    if (!emitter.hasErrors()) {
        ir::AstLowerer lowerer(&emitter);
        (void)lowerer.lower_module(ast);
    }

    collectLspDiagnostics(doc, emitter.getDiagnostics(), lspDiagnostics);

    // 5. Static Linter (keetlint)
    if (!emitter.hasErrors()) {
        auto comments = lexer.getCapturedComments();
        lint::LintConfig lintConfig = lint::LintConfig::discover(fsPath.parent_path());
        lint::LintEngine lintEngine(lintConfig);
        lintEngine.registerDefaultRules();

        auto lintDiags = lintEngine.lintModule(ast, comments, sm, doc.text(), pathStr, universe.get());
        for (const auto& ld : lintDiags) {
            Diagnostic lspDiag;
            lspDiag.severity = convertSeverity(ld.severity);
            lspDiag.code = ld.ruleCode;
            lspDiag.source = "keetlint";
            lspDiag.message = ld.message;
            lspDiag.range = spanToRange(doc, ld.span);
            for (const auto& f : ld.footers) {
                lspDiag.message += "\n" + f.message;
            }
            lspDiagnostics.push_back(std::move(lspDiag));
        }
    }

    return lspDiagnostics;
}

std::optional<Location> Analyzer::findDefinition(
    const Document& doc,
    Position pos,
    const DocumentManager* docManager
) {
    auto byteOffsetOpt = doc.positionToOffset(pos);
    if (!byteOffsetOpt.has_value()) {
        return std::nullopt;
    }
    size_t targetOffset = *byteOffsetOpt;

    diag::SourceManager sm;
    std::string pathStr = uriToPath(doc.uri());
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());

    lexer::Lexer lexer(fileId, doc.text());
    auto tokens = lexer.tokenizeAll();

    // Find token under cursor
    const lexer::Token* targetTok = nullptr;
    for (const auto& tok : tokens) {
        if (targetOffset >= tok.span.start.offset && targetOffset < tok.span.end.offset) {
            targetTok = &tok;
            break;
        }
    }
    if (!targetTok) {
        for (const auto& tok : tokens) {
            if (targetOffset == tok.span.end.offset && tok.span.length() > 0) {
                targetTok = &tok;
                break;
            }
        }
    }

    if (!targetTok) {
        return std::nullopt;
    }

    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();

    // 1. Check if clicking on an import path (e.g. StringLiteral "./aaaa")
    if (targetTok->is(lexer::TokenKind::StringLiteral)) {
        std::string rawPath = std::string(targetTok->lexeme);
        if (rawPath.size() >= 2 && (rawPath.front() == '"' || rawPath.front() == '\'')) {
            rawPath = rawPath.substr(1, rawPath.size() - 2);
        }

        std::filesystem::path projectRoot = driver::findProjectRoot(fsPath);
        resolver::ModulePathResolver pathResolver(projectRoot);
        auto resolved = pathResolver.resolve(fsPath, rawPath, targetTok->span);

        if (resolved.has_value() && !resolved->isStandardLibrary()) {
            std::string destUri = "file://" + resolved->canonicalPath.generic_string();
            Range targetRange{Position{0, 0}, Position{0, 0}};

            // If the target document is already loaded, we can provide a 0-0 range
            if (docManager && docManager->hasDocument(destUri)) {
                auto destDoc = docManager->getDocument(destUri);
                if (destDoc.has_value()) {
                    targetRange = Range{Position{0, 0}, Position{0, 0}};
                }
            }
            return Location{destUri, targetRange};
        }
    }

    // 2. Identifier Go-to-Definition
    if (!targetTok->is(lexer::TokenKind::Identifier)) {
        return std::nullopt;
    }

    std::string ident = std::string(targetTok->lexeme);

    // Parse AST to query declaration spans
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);
    lexer::Lexer pLexer(fileId, doc.text());
    parser::Parser parser(pLexer, &emitter);
    auto ast = parser.parseModule(moduleName);

    // 2a. Check if identifier is an imported package or module (e.g. `import math_utils;`)
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            // Check selective symbols
            for (const auto& sym : imp->getSymbols()) {
                if (sym.getEffectiveName() == ident) {
                    // Jumps to the import declaration
                    return Location{doc.uri(), spanToRange(doc, sym.span.isValid() ? sym.span : imp->getSpan())};
                }
            }

            // Check namespace or bare import
            std::string modAlias = imp->getModuleAlias();
            std::string modPath = imp->getModulePath();
            std::string modStem = std::filesystem::path(modPath).stem().string();
            if (modAlias == ident || modPath == ident || (modAlias.empty() && modStem == ident)) {
                std::filesystem::path projectRoot = driver::findProjectRoot(fsPath);
                resolver::ModulePathResolver pathResolver(projectRoot);
                auto resolved = pathResolver.resolve(fsPath, modPath, imp->getSpan());
                if (resolved.has_value() && !resolved->isStandardLibrary()) {
                    std::string destUri = "file://" + resolved->canonicalPath.generic_string();
                    return Location{destUri, Range{Position{0, 0}, Position{0, 0}}};
                }
            }
        }
    }

    // 2b. Check local parameters within function enclosing cursor
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            diag::SourceSpan fnSpan = fnDecl->getSpan();
            if (targetOffset >= fnSpan.start.offset && targetOffset <= fnSpan.end.offset) {
                // Inside this function! Check its parameters first
                for (const auto& param : fnDecl->getParams()) {
                    if (param.name == ident) {
                        return Location{doc.uri(), spanToRange(doc, param.span)};
                    }
                }
            }
        } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& method : classDecl->getMethods()) {
                if (!method) continue;
                diag::SourceSpan mSpan = method->getSpan();
                if (targetOffset >= mSpan.start.offset && targetOffset <= mSpan.end.offset) {
                    for (const auto& param : method->getParams()) {
                        if (param.name == ident) {
                            return Location{doc.uri(), spanToRange(doc, param.span)};
                        }
                    }
                }
            }
        }
    }

    // 2c. Check top-level declarations (functions, variables, constants, classes, enums)
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (fnDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, fnDecl->getSpan())};
            }
        } else if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            if (varDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, varDecl->getSpan())};
            }
        } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            if (constDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, constDecl->getSpan())};
            }
        } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            if (defDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, defDecl->getSpan())};
            }
        } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            if (classDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, classDecl->getSpan())};
            }
            // Check class fields
            for (const auto& field : classDecl->getFields()) {
                if (field.name == ident) {
                    return Location{doc.uri(), spanToRange(doc, field.span)};
                }
            }
            // Check class methods
            for (const auto& method : classDecl->getMethods()) {
                if (method && method->getName() == ident) {
                    return Location{doc.uri(), spanToRange(doc, method->getSpan())};
                }
            }
        } else if (const auto* enumDecl = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
            if (enumDecl->getName() == ident) {
                return Location{doc.uri(), spanToRange(doc, enumDecl->getSpan())};
            }
            for (const auto& member : enumDecl->getMembers()) {
                if (member.name == ident) {
                    return Location{doc.uri(), spanToRange(doc, member.span)};
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<Hover> Analyzer::getHover(
    const Document& doc,
    Position pos,
    const DocumentManager* /*docManager*/
) {
    auto byteOffsetOpt = doc.positionToOffset(pos);
    if (!byteOffsetOpt.has_value()) {
        return std::nullopt;
    }
    size_t targetOffset = *byteOffsetOpt;

    diag::SourceManager sm;
    std::string pathStr = uriToPath(doc.uri());
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());

    lexer::Lexer lexer(fileId, doc.text());
    auto tokens = lexer.tokenizeAll();

    // Find token under cursor
    const lexer::Token* targetTok = nullptr;
    for (const auto& tok : tokens) {
        if (targetOffset >= tok.span.start.offset && targetOffset < tok.span.end.offset) {
            targetTok = &tok;
            break;
        }
    }
    if (!targetTok) {
        for (const auto& tok : tokens) {
            if (targetOffset == tok.span.end.offset && tok.span.length() > 0) {
                targetTok = &tok;
                break;
            }
        }
    }

    if (!targetTok) {
        return std::nullopt;
    }

    Range tokRange = spanToRange(doc, targetTok->span);

    // 1. Language Keywords & Primitive Types
    if (auto kwDoc = getKeywordDoc(targetTok->kind); kwDoc.has_value()) {
        return Hover{MarkupContent{"markdown", *kwDoc}, tokRange};
    }

    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();

    // 2. Relative Module Import (String Literal)
    if (targetTok->is(lexer::TokenKind::StringLiteral)) {
        std::string rawPath = std::string(targetTok->lexeme);
        if (rawPath.size() >= 2 && (rawPath.front() == '"' || rawPath.front() == '\'')) {
            rawPath = rawPath.substr(1, rawPath.size() - 2);
        }

        std::filesystem::path projectRoot = driver::findProjectRoot(fsPath);
        resolver::ModulePathResolver pathResolver(projectRoot);
        auto resolved = pathResolver.resolve(fsPath, rawPath, targetTok->span);

        if (resolved.has_value() && !resolved->isStandardLibrary()) {
            std::string content = "```keet\nmodule \"" + rawPath + "\"\n```\n**Path:** `" +
                                  resolved->canonicalPath.string() + "`";
            return Hover{MarkupContent{"markdown", std::move(content)}, tokRange};
        }
    }

    // 3. Identifier Hover
    if (!targetTok->is(lexer::TokenKind::Identifier)) {
        return std::nullopt;
    }

    std::string ident = std::string(targetTok->lexeme);

    // 3a. Standard Library Modules
    if (ident == "io") {
        return Hover{MarkupContent{"markdown", "```keet\nimport io;\n```\n**Standard Library Module: `io`**\n\nInput and output operations, terminal formatting, and standard streams."}, tokRange};
    }
    if (ident == "fs") {
        return Hover{MarkupContent{"markdown", "```keet\nimport fs;\n```\n**Standard Library Module: `fs`**\n\nFile system operations: reading, writing, and querying files and directories."}, tokRange};
    }
    if (ident == "sys") {
        return Hover{MarkupContent{"markdown", "```keet\nimport sys;\n```\n**Standard Library Module: `sys`**\n\nSystem information, environment variables, command-line arguments, and process control."}, tokRange};
    }
    if (ident == "math") {
        return Hover{MarkupContent{"markdown", "```keet\nimport math;\n```\n**Standard Library Module: `math`**\n\nMathematical constants and standard mathematical functions."}, tokRange};
    }
    if (ident == "time") {
        return Hover{MarkupContent{"markdown", "```keet\nimport time;\n```\n**Standard Library Module: `time`**\n\nTime measurement, formatting, sleep, and timestamps."}, tokRange};
    }
    if (ident == "json") {
        return Hover{MarkupContent{"markdown", "```keet\nimport json;\n```\n**Standard Library Module: `json`**\n\nJSON serialization and parsing operations."}, tokRange};
    }
    if (ident == "net") {
        return Hover{MarkupContent{"markdown", "```keet\nimport net;\n```\n**Standard Library Module: `net`**\n\nNetworking, HTTP requests, TCP/UDP sockets."}, tokRange};
    }
    if (ident == "test") {
        return Hover{MarkupContent{"markdown", "```keet\nimport test;\n```\n**Standard Library Module: `test`**\n\nUnit testing framework, assertion primitives, and test suite runner."}, tokRange};
    }
    if (ident == "crypto") {
        return Hover{MarkupContent{"markdown", "```keet\nimport crypto;\n```\n**Standard Library Module: `crypto`**\n\nCryptographic hashing, HMAC, symmetric encryption (AEAD), CSPRNG, encodings, and symbology (QR & barcodes)."}, tokRange};
    }

    // Parse AST to query user declarations
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);
    lexer::Lexer pLexer(fileId, doc.text());
    parser::Parser parser(pLexer, &emitter);
    auto ast = parser.parseModule(moduleName);

    // 3c. Function Parameters inside enclosing function/method
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            diag::SourceSpan fnSpan = fnDecl->getSpan();
            if (targetOffset >= fnSpan.start.offset && targetOffset <= fnSpan.end.offset) {
                for (const auto& param : fnDecl->getParams()) {
                    if (param.name == ident) {
                        std::string sig = "(parameter) " + std::string(param.isMut ? "mut " : "") + param.name + ": " + formatType(param.type.get(), "any");
                        return Hover{MarkupContent{"markdown", "```keet\n" + sig + "\n```"}, tokRange};
                    }
                }
            }
        } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& method : classDecl->getMethods()) {
                if (!method) continue;
                diag::SourceSpan mSpan = method->getSpan();
                if (targetOffset >= mSpan.start.offset && targetOffset <= mSpan.end.offset) {
                    for (const auto& param : method->getParams()) {
                        if (param.name == ident) {
                            std::string sig = "(parameter) " + std::string(param.isMut ? "mut " : "") + param.name + ": " + formatType(param.type.get(), "any");
                            return Hover{MarkupContent{"markdown", "```keet\n" + sig + "\n```"}, tokRange};
                        }
                    }
                }
            }
        }
    }

    // 3d. Check Top-Level User Declarations
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (fnDecl->getName() == ident) {
                std::string sig = (fnDecl->isExported() ? "export " : "");
                if (fnDecl->isAsync()) sig += "async ";
                sig += "fn " + fnDecl->getName() + "(";
                for (size_t i = 0; i < fnDecl->getParams().size(); ++i) {
                    if (i > 0) sig += ", ";
                    const auto& p = fnDecl->getParams()[i];
                    if (p.isMut) sig += "mut ";
                    sig += p.name + ": " + formatType(p.type.get(), "any");
                }
                sig += "): " + formatType(fnDecl->getReturnType(), "void");

                std::string md = "```keet\n" + sig + "\n```";
                std::string docComment = extractDocComments(doc.text(), fnDecl->getSpan().start.offset);
                if (!docComment.empty()) {
                    md += "\n" + docComment;
                }
                return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
            }
        } else if (const auto* varDecl = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            if (varDecl->getName() == ident) {
                std::string sig = (varDecl->isExported() ? "export let " : "let ") + varDecl->getName() + ": " + formatType(varDecl->getType(), "inferred");
                std::string md = "```keet\n" + sig + "\n```";
                std::string docComment = extractDocComments(doc.text(), varDecl->getSpan().start.offset);
                if (!docComment.empty()) {
                    md += "\n" + docComment;
                }
                return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
            }
        } else if (const auto* constDecl = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            if (constDecl->getName() == ident) {
                std::string sig = (constDecl->isExported() ? "export const " : "const ") + constDecl->getName() + ": " + formatType(constDecl->getType(), "inferred");
                std::string md = "```keet\n" + sig + "\n```";
                std::string docComment = extractDocComments(doc.text(), constDecl->getSpan().start.offset);
                if (!docComment.empty()) {
                    md += "\n" + docComment;
                }
                return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
            }
        } else if (const auto* defDecl = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            if (defDecl->getName() == ident) {
                std::string sig = (defDecl->isExported() ? "export define " : "define ") + defDecl->getName();
                if (defDecl->getType()) {
                    sig += ": " + formatType(defDecl->getType());
                }
                return Hover{MarkupContent{"markdown", "```keet\n" + sig + "\n```"}, tokRange};
            }
        } else if (const auto* classDecl = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            if (classDecl->getName() == ident) {
                std::string sig = (classDecl->isExported() ? "export class " : "class ") + classDecl->getName();
                if (classDecl->hasBaseClass()) {
                    sig += " inherits " + classDecl->getBaseClass();
                }
                std::string md = "```keet\n" + sig + "\n```\n*Class with " +
                                 std::to_string(classDecl->getFields().size()) + " field(s) and " +
                                 std::to_string(classDecl->getMethods().size()) + " method(s).*";
                std::string docComment = extractDocComments(doc.text(), classDecl->getSpan().start.offset);
                if (!docComment.empty()) {
                    md += "\n\n" + docComment;
                }
                return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
            }
            // Check class fields
            for (const auto& field : classDecl->getFields()) {
                if (field.name == ident) {
                    std::string sig = "(field) ";
                    if (field.visibility == ast::Visibility::Pub) sig += "pub ";
                    else if (field.visibility == ast::Visibility::Priv) sig += "priv ";
                    sig += std::string(field.isConst ? "const " : "let ") + field.name + ": " + formatType(field.type.get(), "any");
                    return Hover{MarkupContent{"markdown", "```keet\n" + sig + "\n```"}, tokRange};
                }
            }
            // Check class methods
            for (const auto& method : classDecl->getMethods()) {
                if (method && method->getName() == ident) {
                    std::string sig = "(method) ";
                    if (method->getVisibility() == ast::Visibility::Pub) sig += "pub ";
                    else if (method->getVisibility() == ast::Visibility::Priv) sig += "priv ";
                    if (method->isAsync()) sig += "async ";
                    sig += "fn " + method->getName() + "(";
                    for (size_t i = 0; i < method->getParams().size(); ++i) {
                        if (i > 0) sig += ", ";
                        const auto& p = method->getParams()[i];
                        if (p.isMut) sig += "mut ";
                        sig += p.name + ": " + formatType(p.type.get(), "any");
                    }
                    sig += "): " + formatType(method->getReturnType(), "void");

                    std::string md = "```keet\n" + sig + "\n```";
                    std::string docComment = extractDocComments(doc.text(), method->getSpan().start.offset);
                    if (!docComment.empty()) {
                        md += "\n" + docComment;
                    }
                    return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
                }
            }
        } else if (const auto* enumDecl = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
            if (enumDecl->getName() == ident) {
                std::string sig = (enumDecl->isExported() ? "export enum " : "enum ") + enumDecl->getName();
                std::string md = "```keet\n" + sig + "\n```\n*Enum with " +
                                 std::to_string(enumDecl->getMembers().size()) + " variant(s).*";
                std::string docComment = extractDocComments(doc.text(), enumDecl->getSpan().start.offset);
                if (!docComment.empty()) {
                    md += "\n\n" + docComment;
                }
                return Hover{MarkupContent{"markdown", std::move(md)}, tokRange};
            }
            for (const auto& member : enumDecl->getMembers()) {
                if (member.name == ident) {
                    std::string sig = "(enum member) " + enumDecl->getName() + "." + member.name;
                    return Hover{MarkupContent{"markdown", "```keet\n" + sig + "\n```"}, tokRange};
                }
            }
        }
    }

    // 3e. Standard Library Built-in Functions/Constants
    if (auto stdDoc = getStdlibMemberDoc(ident); stdDoc.has_value()) {
        std::string content = "```keet\n" + stdDoc->signature + "\n```\n" + stdDoc->doc;
        return Hover{MarkupContent{"markdown", std::move(content)}, tokRange};
    }

    return std::nullopt;
}

CompletionList Analyzer::getCompletions(
    const Document& doc,
    Position pos,
    const DocumentManager* docManager
) {
    CompletionList list;
    list.isIncomplete = false;

    auto byteOffsetOpt = doc.positionToOffset(pos);
    if (!byteOffsetOpt.has_value()) {
        return list;
    }
    size_t targetOffset = *byteOffsetOpt;
    const std::string& text = doc.text();
    if (targetOffset > text.size()) {
        targetOffset = text.size();
    }

    std::string pathStr = uriToPath(doc.uri());
    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();

    // Line prefix up to cursor
    size_t lineStart = targetOffset;
    while (lineStart > 0 && text[lineStart - 1] != '\n') {
        lineStart--;
    }
    std::string linePrefix = text.substr(lineStart, targetOffset - lineStart);

    // 1. Check if completing an import path (inside quotes following `import` or `from`)
    size_t kwImport = linePrefix.rfind("import");
    size_t kwFrom = linePrefix.rfind("from");
    size_t kwPos = std::string::npos;
    if (kwImport != std::string::npos && (kwFrom == std::string::npos || kwImport > kwFrom)) {
        kwPos = kwImport;
    } else if (kwFrom != std::string::npos) {
        kwPos = kwFrom;
    }

    if (kwPos != std::string::npos) {
        int singleQuotes = 0;
        int doubleQuotes = 0;
        for (size_t i = kwPos; i < linePrefix.size(); ++i) {
            if (linePrefix[i] == '\'') singleQuotes++;
            else if (linePrefix[i] == '"') doubleQuotes++;
        }
        if ((singleQuotes % 2 == 1) || (doubleQuotes % 2 == 1)) {
            std::filesystem::path currentDir = fsPath.parent_path();
            if (currentDir.empty()) {
                std::error_code ec;
                currentDir = std::filesystem::current_path(ec);
            }

            std::unordered_set<std::string> seen;
            std::error_code ec;
            if (std::filesystem::exists(currentDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(currentDir, ec)) {
                    if (entry.is_regular_file(ec) && entry.path().extension() == ".keet") {
                        if (entry.path() != fsPath) {
                            std::string stem = entry.path().stem().string();
                            std::string fn = entry.path().filename().string();
                            if (seen.insert("./" + stem).second) {
                                CompletionItem item;
                                item.label = "./" + stem;
                                item.kind = CompletionItemKind::File;
                                item.detail = "KeetScript module (" + fn + ")";
                                item.insertText = "./" + stem;
                                list.items.push_back(std::move(item));
                            }
                        }
                    }
                }
            }

            if (docManager) {
                for (const auto& openUri : docManager->openUris()) {
                    if (openUri == doc.uri()) continue;
                    std::string openPath = uriToPath(openUri);
                    std::filesystem::path op(openPath);
                    if (op.parent_path() == currentDir && op.extension() == ".keet") {
                        std::string stem = op.stem().string();
                        if (seen.insert("./" + stem).second) {
                            CompletionItem item;
                            item.label = "./" + stem;
                            item.kind = CompletionItemKind::File;
                            item.detail = "Open KeetScript module (" + op.filename().string() + ")";
                            item.insertText = "./" + stem;
                            list.items.push_back(std::move(item));
                        }
                    }
                }
            }

            return list;
        }
    }

    // 2. Check for Dot-Completion (`receiver.`)
    size_t p = targetOffset;
    while (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
        p--;
    }
    while (p > 0 && (text[p - 1] == ' ' || text[p - 1] == '\t')) {
        p--;
    }
    bool isDot = (p > 0 && text[p - 1] == '.');

    if (isDot) {
        p--; // Skip '.'
        while (p > 0 && (text[p - 1] == ' ' || text[p - 1] == '\t')) {
            p--;
        }
        size_t receiverEnd = p;
        while (p > 0 && (std::isalnum(static_cast<unsigned char>(text[p - 1])) || text[p - 1] == '_')) {
            p--;
        }
        std::string receiver = text.substr(p, receiverEnd - p);

        // Check if there was another qualifier dot before receiver (e.g. `io.Color.`)
        std::string qualifier = "";
        size_t q = p;
        while (q > 0 && (text[q - 1] == ' ' || text[q - 1] == '\t')) q--;
        if (q > 0 && text[q - 1] == '.') {
            q--;
            while (q > 0 && (text[q - 1] == ' ' || text[q - 1] == '\t')) q--;
            size_t qEnd = q;
            while (q > 0 && (std::isalnum(static_cast<unsigned char>(text[q - 1])) || text[q - 1] == '_')) q--;
            qualifier = text.substr(q, qEnd - q);
        }

        // 2a. Standard library module dot completion: `io.`, `fs.`, `math.`, etc.
        if (sema::isStdlibModule(receiver)) {
            auto scope = sema::createStdlibModuleScope(receiver);
            if (scope) {
                for (const auto& [name, sym] : scope->getSymbols()) {
                    CompletionItem item;
                    item.label = name;
                    if (sym->getKind() == sema::SymbolKind::Function) {
                        item.kind = CompletionItemKind::Function;
                    } else if (sym->getKind() == sema::SymbolKind::Constant) {
                        item.kind = CompletionItemKind::Constant;
                    } else if (sym->getKind() == sema::SymbolKind::Enum) {
                        item.kind = CompletionItemKind::Enum;
                    } else if (sym->getKind() == sema::SymbolKind::EnumValue) {
                        item.kind = CompletionItemKind::EnumMember;
                    } else {
                        item.kind = CompletionItemKind::Field;
                    }

                    if (auto docOpt = getStdlibMemberDoc(name)) {
                        item.detail = docOpt->signature;
                        item.documentation = docOpt->doc;
                    } else {
                        item.detail = receiver + "." + name;
                    }
                    list.items.push_back(std::move(item));
                }
                std::sort(list.items.begin(), list.items.end(), [](const CompletionItem& a, const CompletionItem& b) {
                    return a.label < b.label;
                });
                return list;
            }
        }

        // 2b. Special stdlib sub-enum dot completion: `io.Color.`
        if (qualifier == "io" && receiver == "Color") {
            const std::vector<std::string> colorMembers = {
                "Default", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White", "Gray"
            };
            for (const auto& m : colorMembers) {
                CompletionItem item;
                item.label = m;
                item.kind = CompletionItemKind::EnumMember;
                item.detail = "(enum member) io.Color." + m;
                list.items.push_back(std::move(item));
            }
            return list;
        }

        // Parse AST to resolve classes, enums, 'this', and variables
        diag::SourceManager sm;
        diag::FileId fileId = sm.addBuffer(pathStr, text);
        diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);
        lexer::Lexer pLexer(fileId, text);
        parser::Parser parser(pLexer, &emitter);
        auto ast = parser.parseModule(moduleName);

        // 2c. `this.` dot completion
        if (receiver == "this") {
            const ast::ClassDecl* enclosingClass = nullptr;
            for (const auto& decl : ast.declarations) {
                if (!decl) continue;
                if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
                    if (targetOffset >= cd->getSpan().start.offset && targetOffset <= cd->getSpan().end.offset) {
                        enclosingClass = cd;
                        break;
                    }
                }
            }

            if (enclosingClass) {
                for (const auto& field : enclosingClass->getFields()) {
                    CompletionItem item;
                    item.label = field.name;
                    item.kind = CompletionItemKind::Field;
                    std::string sig;
                    if (field.visibility == ast::Visibility::Pub) sig += "pub ";
                    else if (field.visibility == ast::Visibility::Priv) sig += "priv ";
                    sig += std::string(field.isConst ? "const " : "let ") + field.name + ": " + formatType(field.type.get(), "any");
                    item.detail = sig;
                    list.items.push_back(std::move(item));
                }
                for (const auto& method : enclosingClass->getMethods()) {
                    if (!method) continue;
                    CompletionItem item;
                    item.label = method->getName();
                    item.kind = CompletionItemKind::Method;
                    std::string sig = "(method) ";
                    if (method->getVisibility() == ast::Visibility::Pub) sig += "pub ";
                    else if (method->getVisibility() == ast::Visibility::Priv) sig += "priv ";
                    if (method->isAsync()) sig += "async ";
                    sig += "fn " + method->getName() + "(";
                    for (size_t i = 0; i < method->getParams().size(); ++i) {
                        if (i > 0) sig += ", ";
                        const auto& param = method->getParams()[i];
                        if (param.isMut) sig += "mut ";
                        sig += param.name + ": " + formatType(param.type.get(), "any");
                    }
                    sig += "): " + formatType(method->getReturnType(), "void");
                    item.detail = sig;
                    item.documentation = extractDocComments(text, method->getSpan().start.offset);
                    list.items.push_back(std::move(item));
                }
                return list;
            }
        }

        // 2d. Enum variant completion: `Status.`
        for (const auto& decl : ast.declarations) {
            if (!decl) continue;
            if (const auto* ed = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
                if (ed->getName() == receiver) {
                    for (const auto& member : ed->getMembers()) {
                        CompletionItem item;
                        item.label = member.name;
                        item.kind = CompletionItemKind::EnumMember;
                        item.detail = "(enum member) " + ed->getName() + "." + member.name;
                        list.items.push_back(std::move(item));
                    }
                    return list;
                }
            }
        }

        // 2e. Instance variable dot completion: `u.` where `let u = User();` or `fn test(u: User)`
        std::string inferredClass = findReceiverClassName(ast, receiver, targetOffset);
        if (inferredClass.empty()) {
            inferredClass = receiver;
        }

        for (const auto& decl : ast.declarations) {
            if (!decl) continue;
            if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
                if (cd->getName() == inferredClass) {
                    for (const auto& field : cd->getFields()) {
                        CompletionItem item;
                        item.label = field.name;
                        item.kind = CompletionItemKind::Field;
                        item.detail = (field.isConst ? "const " : "let ") + field.name + ": " + formatType(field.type.get(), "any");
                        list.items.push_back(std::move(item));
                    }
                    for (const auto& method : cd->getMethods()) {
                        if (!method) continue;
                        CompletionItem item;
                        item.label = method->getName();
                        item.kind = CompletionItemKind::Method;
                        std::string sig = "fn " + method->getName() + "(";
                        for (size_t i = 0; i < method->getParams().size(); ++i) {
                            if (i > 0) sig += ", ";
                            const auto& param = method->getParams()[i];
                            if (param.isMut) sig += "mut ";
                            sig += param.name + ": " + formatType(param.type.get(), "any");
                        }
                        sig += "): " + formatType(method->getReturnType(), "void");
                        item.detail = sig;
                        item.documentation = extractDocComments(text, method->getSpan().start.offset);
                        list.items.push_back(std::move(item));
                    }
                    return list;
                }
            }
        }

        return list;
    }

    // 3. General Scope Completions (Keywords, Snippets, Types, Globals, Locals, Top-Level)
    std::unordered_set<std::string> seen;

    // 3a. Snippets
    auto addSnippet = [&](std::string label, std::string insertText, std::string detail) {
        if (seen.insert(label).second) {
            CompletionItem item;
            item.label = std::move(label);
            item.insertText = std::move(insertText);
            item.insertTextFormat = InsertTextFormat::Snippet;
            item.kind = CompletionItemKind::Snippet;
            item.detail = std::move(detail);
            list.items.push_back(std::move(item));
        }
    };
    addSnippet("fn (snippet)", "fn ${1:name}(${2}): ${3:void} {\n\t${0}\n}", "Function declaration snippet");
    addSnippet("for (snippet)", "for ${1:item} in ${2:collection} {\n\t${0}\n}", "For-in iteration loop snippet");
    addSnippet("if (snippet)", "if ${1:condition} {\n\t${0}\n}", "Conditional statement snippet");
    addSnippet("class (snippet)", "class ${1:Name} {\n\t${0}\n}", "Class declaration snippet");
    addSnippet("main (snippet)", "fn main(): void {\n\t${0}\n}", "Main entrypoint snippet");

    // 3b. Keywords
    static const std::vector<std::string> keywords = {
        "fn", "let", "mut", "const", "class", "enum", "import", "export", "pub", "priv",
        "if", "else", "elif", "for", "in", "while", "return", "zonk", "switch", "case",
        "default", "break", "continue", "inherits", "typeof", "as", "from", "errtype"
    };
    for (const auto& kw : keywords) {
        if (seen.insert(kw).second) {
            CompletionItem item;
            item.label = kw;
            item.kind = CompletionItemKind::Keyword;
            item.detail = "keyword";
            auto tk = kwToTokenKind(kw);
            if (auto kwDoc = getKeywordDoc(tk)) {
                item.documentation = *kwDoc;
            }
            list.items.push_back(std::move(item));
        }
    }

    // 3c. Primitive Types
    static const std::vector<std::string> primitives = {
        "int", "float", "string", "bool", "char", "void", "error", "array", "map", "any"
    };
    for (const auto& prim : primitives) {
        if (seen.insert(prim).second) {
            CompletionItem item;
            item.label = prim;
            item.kind = CompletionItemKind::Keyword;
            item.detail = "primitive type";
            list.items.push_back(std::move(item));
        }
    }

    // 3d. Predeclared Constants
    static const std::vector<std::string> constants = {"true", "false", "null"};
    for (const auto& c : constants) {
        if (seen.insert(c).second) {
            CompletionItem item;
            item.label = c;
            item.kind = CompletionItemKind::Constant;
            item.detail = "built-in constant";
            list.items.push_back(std::move(item));
        }
    }

    // 3e. Standard Library Modules
    static const std::vector<std::pair<std::string, std::string>> stdModules = {
        {"io", "Standard Library Module: `io` (Console and standard streams)"},
        {"fs", "Standard Library Module: `fs` (File system operations)"},
        {"sys", "Standard Library Module: `sys` (System and process information)"},
        {"math", "Standard Library Module: `math` (Mathematical functions and constants)"},
        {"time", "Standard Library Module: `time` (Time measurement and formatting)"},
        {"json", "Standard Library Module: `json` (JSON serialization and parsing)"},
        {"net", "Standard Library Module: `net` (Networking, HTTP and sockets)"},
        {"test", "Standard Library Module: `test` (Unit testing and assertions)"},
        {"crypto", "Standard Library Module: `crypto` (Hashing, HMAC, AEAD ciphers, CSPRNG, and QR/barcodes)"},
        {"ui", "Standard Library Module: `ui` (Universal Reactive UI, TUI, Flexbox, and Web VDOM)"}
    };
    for (const auto& [mod, desc] : stdModules) {
        if (seen.insert(mod).second) {
            CompletionItem item;
            item.label = mod;
            item.kind = CompletionItemKind::Module;
            item.detail = "Standard library module";
            item.documentation = desc;
            list.items.push_back(std::move(item));
        }
    }

    // 3f. KML Runes, Primitives and Directives
    static const std::vector<std::pair<std::string, std::string>> kmlRunes = {
        {"$state", "KML State Rune: `$state(initialValue)` creates reactive local state"},
        {"$derived", "KML Derived Rune: `$derived(expression)` creates computed reactive state"},
        {"$effect", "KML Effect Rune: `$effect(() => { ... })` creates reactive side effect"},
        {"$props", "KML Props Rune: `$props()` retrieves component props"},
        {"$bind", "KML Two-Way Binding: `$bind.value={state}` binds property bidirectionally"}
    };
    for (const auto& [rune, kmlDoc] : kmlRunes) {
        if (seen.insert(rune).second) {
            CompletionItem item;
            item.label = rune;
            item.kind = CompletionItemKind::Keyword;
            item.detail = "KML Rune";
            item.documentation = kmlDoc;
            list.items.push_back(std::move(item));
        }
    }

    static const std::vector<std::string> kmlTags = {
        "box", "vstack", "hstack", "text", "button", "input", "scroll", "slot"
    };
    for (const auto& tag : kmlTags) {
        if (seen.insert(tag).second) {
            CompletionItem item;
            item.label = tag;
            item.kind = CompletionItemKind::Class;
            item.detail = "KML Layout Primitive";
            list.items.push_back(std::move(item));
        }
    }

    // Parse AST to gather local scope and top-level definitions
    diag::SourceManager sm;
    diag::FileId fileId = sm.addBuffer(pathStr, text);
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);
    lexer::Lexer pLexer(fileId, text);
    parser::Parser parser(pLexer, &emitter);
    auto ast = parser.parseModule(moduleName);

    // 3f. Function parameters and local variables if inside a function/method
    bool insideClassMethod = false;
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (targetOffset >= fnDecl->getSpan().start.offset && targetOffset <= fnDecl->getSpan().end.offset) {
                for (const auto& param : fnDecl->getParams()) {
                    if (seen.insert(param.name).second) {
                        CompletionItem item;
                        item.label = param.name;
                        item.kind = CompletionItemKind::Variable;
                        item.detail = "(parameter) " + std::string(param.isMut ? "mut " : "") + param.name + ": " + formatType(param.type.get(), "any");
                        list.items.push_back(std::move(item));
                    }
                }
                collectLocalBindings(fnDecl->getBody(), targetOffset, list.items, seen);
            }
        } else if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            for (const auto& method : cd->getMethods()) {
                if (method && targetOffset >= method->getSpan().start.offset && targetOffset <= method->getSpan().end.offset) {
                    insideClassMethod = true;
                    for (const auto& param : method->getParams()) {
                        if (seen.insert(param.name).second) {
                            CompletionItem item;
                            item.label = param.name;
                            item.kind = CompletionItemKind::Variable;
                            item.detail = "(parameter) " + std::string(param.isMut ? "mut " : "") + param.name + ": " + formatType(param.type.get(), "any");
                            list.items.push_back(std::move(item));
                        }
                    }
                    collectLocalBindings(method->getBody(), targetOffset, list.items, seen);
                }
            }
        }
    }

    if (insideClassMethod) {
        if (seen.insert("this").second) {
            CompletionItem item;
            item.label = "this";
            item.kind = CompletionItemKind::Keyword;
            item.detail = "Current class instance";
            list.items.push_back(std::move(item));
        }
        if (seen.insert("inherited").second) {
            CompletionItem item;
            item.label = "inherited";
            item.kind = CompletionItemKind::Keyword;
            item.detail = "Base class member access";
            list.items.push_back(std::move(item));
        }
    }

    // 3g. Top-level AST declarations
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            if (seen.insert(fnDecl->getName()).second) {
                CompletionItem item;
                item.label = fnDecl->getName();
                item.kind = CompletionItemKind::Function;
                std::string sig = (fnDecl->isExported() ? "export " : "") + std::string("fn ") + fnDecl->getName() + "(";
                for (size_t i = 0; i < fnDecl->getParams().size(); ++i) {
                    if (i > 0) sig += ", ";
                    const auto& param = fnDecl->getParams()[i];
                    if (param.isMut) sig += "mut ";
                    sig += param.name + ": " + formatType(param.type.get(), "any");
                }
                sig += "): " + formatType(fnDecl->getReturnType(), "void");
                item.detail = sig;
                item.documentation = extractDocComments(text, fnDecl->getSpan().start.offset);
                list.items.push_back(std::move(item));
            }
        } else if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            if (seen.insert(cd->getName()).second) {
                CompletionItem item;
                item.label = cd->getName();
                item.kind = CompletionItemKind::Class;
                item.detail = (cd->isExported() ? "export class " : "class ") + cd->getName();
                item.documentation = extractDocComments(text, cd->getSpan().start.offset);
                list.items.push_back(std::move(item));
            }
        } else if (const auto* ed = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
            if (seen.insert(ed->getName()).second) {
                CompletionItem item;
                item.label = ed->getName();
                item.kind = CompletionItemKind::Enum;
                item.detail = (ed->isExported() ? "export enum " : "enum ") + ed->getName();
                item.documentation = extractDocComments(text, ed->getSpan().start.offset);
                list.items.push_back(std::move(item));
            }
        } else if (const auto* vd = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            if (seen.insert(vd->getName()).second) {
                CompletionItem item;
                item.label = vd->getName();
                item.kind = CompletionItemKind::Variable;
                item.detail = (vd->isExported() ? "export let " : "let ") + vd->getName() + ": " + formatType(vd->getType(), "any");
                list.items.push_back(std::move(item));
            }
        } else if (const auto* cnd = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            if (seen.insert(cnd->getName()).second) {
                CompletionItem item;
                item.label = cnd->getName();
                item.kind = CompletionItemKind::Constant;
                item.detail = (cnd->isExported() ? "export const " : "const ") + cnd->getName() + ": " + formatType(cnd->getType(), "any");
                list.items.push_back(std::move(item));
            }
        } else if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            for (const auto& sym : imp->getSymbols()) {
                if (seen.insert(sym.getEffectiveName()).second) {
                    CompletionItem item;
                    item.label = sym.getEffectiveName();
                    item.kind = CompletionItemKind::Reference;
                    item.detail = "Imported from \"" + imp->getModulePath() + "\"";
                    list.items.push_back(std::move(item));
                }
            }
            if (!imp->getModuleAlias().empty() && seen.insert(imp->getModuleAlias()).second) {
                CompletionItem item;
                item.label = imp->getModuleAlias();
                item.kind = CompletionItemKind::Module;
                item.detail = "Module alias for \"" + imp->getModulePath() + "\"";
                list.items.push_back(std::move(item));
            }
        }
    }

    return list;
}

std::vector<DocumentSymbol> Analyzer::getDocumentSymbols(const Document& doc) {
    std::vector<DocumentSymbol> symbols;

    std::string pathStr = uriToPath(doc.uri());
    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();

    diag::SourceManager sm;
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);
    lexer::Lexer pLexer(fileId, doc.text());
    parser::Parser parser(pLexer, &emitter);
    auto ast = parser.parseModule(moduleName);

    for (const auto& decl : ast.declarations) {
        if (!decl) continue;

        if (const auto* cd = dynamic_cast<const ast::ClassDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = cd->getName();
            sym.kind = SymbolKind::Class;
            sym.range = spanToRange(doc, cd->getSpan());
            sym.selectionRange = findIdentifierRange(doc, cd->getName(), cd->getSpan());
            if (cd->hasBaseClass()) {
                sym.detail = "inherits " + cd->getBaseClass();
            } else {
                sym.detail = (cd->isExported() ? "export class" : "class");
            }

            // Class Fields
            for (const auto& field : cd->getFields()) {
                DocumentSymbol fSym;
                fSym.name = field.name;
                fSym.kind = field.isConst ? SymbolKind::Constant : SymbolKind::Field;
                fSym.range = spanToRange(doc, field.span);
                fSym.selectionRange = findIdentifierRange(doc, field.name, field.span);
                std::string fSig;
                if (field.visibility == ast::Visibility::Pub) fSig += "pub ";
                else if (field.visibility == ast::Visibility::Priv) fSig += "priv ";
                fSig += std::string(field.isConst ? "const " : "let ") + field.name + ": " + formatType(field.type.get(), "any");
                fSym.detail = std::move(fSig);
                sym.children.push_back(std::move(fSym));
            }

            // Class Methods
            for (const auto& method : cd->getMethods()) {
                if (!method) continue;
                DocumentSymbol mSym;
                mSym.name = method->getName();
                mSym.kind = (method->getName() == cd->getName()) ? SymbolKind::Constructor : SymbolKind::Method;
                mSym.range = spanToRange(doc, method->getSpan());
                mSym.selectionRange = findIdentifierRange(doc, method->getName(), method->getSpan());
                std::string mSig;
                if (method->getVisibility() == ast::Visibility::Pub) mSig += "pub ";
                else if (method->getVisibility() == ast::Visibility::Priv) mSig += "priv ";
                if (method->isAsync()) mSig += "async ";
                mSig += "fn " + method->getName() + "(";
                for (size_t i = 0; i < method->getParams().size(); ++i) {
                    if (i > 0) mSig += ", ";
                    const auto& param = method->getParams()[i];
                    if (param.isMut) mSig += "mut ";
                    mSig += param.name + ": " + formatType(param.type.get(), "any");
                }
                mSig += "): " + formatType(method->getReturnType(), "void");
                mSym.detail = std::move(mSig);
                sym.children.push_back(std::move(mSym));
            }

            symbols.push_back(std::move(sym));
        } else if (const auto* fnDecl = dynamic_cast<const ast::FnDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = fnDecl->getName();
            sym.kind = SymbolKind::Function;
            sym.range = spanToRange(doc, fnDecl->getSpan());
            sym.selectionRange = findIdentifierRange(doc, fnDecl->getName(), fnDecl->getSpan());
            std::string sig = (fnDecl->isExported() ? "export " : "") + std::string(fnDecl->isAsync() ? "async " : "") + "fn " + fnDecl->getName() + "(";
            for (size_t i = 0; i < fnDecl->getParams().size(); ++i) {
                if (i > 0) sig += ", ";
                const auto& param = fnDecl->getParams()[i];
                if (param.isMut) sig += "mut ";
                sig += param.name + ": " + formatType(param.type.get(), "any");
            }
            sig += "): " + formatType(fnDecl->getReturnType(), "void");
            sym.detail = std::move(sig);
            symbols.push_back(std::move(sym));
        } else if (const auto* ed = dynamic_cast<const ast::EnumDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = ed->getName();
            sym.kind = SymbolKind::Enum;
            sym.range = spanToRange(doc, ed->getSpan());
            sym.selectionRange = findIdentifierRange(doc, ed->getName(), ed->getSpan());
            sym.detail = (ed->isExported() ? "export enum" : "enum");

            for (const auto& member : ed->getMembers()) {
                DocumentSymbol memSym;
                memSym.name = member.name;
                memSym.kind = SymbolKind::EnumMember;
                memSym.range = spanToRange(doc, member.span);
                memSym.selectionRange = findIdentifierRange(doc, member.name, member.span);
                sym.children.push_back(std::move(memSym));
            }

            symbols.push_back(std::move(sym));
        } else if (const auto* vd = dynamic_cast<const ast::VarDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = vd->getName();
            sym.kind = SymbolKind::Variable;
            sym.range = spanToRange(doc, vd->getSpan());
            sym.selectionRange = findIdentifierRange(doc, vd->getName(), vd->getSpan());
            sym.detail = (vd->isExported() ? "export let " : "let ") + vd->getName() + ": " + formatType(vd->getType(), "any");
            symbols.push_back(std::move(sym));
        } else if (const auto* cnd = dynamic_cast<const ast::ConstDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = cnd->getName();
            sym.kind = SymbolKind::Constant;
            sym.range = spanToRange(doc, cnd->getSpan());
            sym.selectionRange = findIdentifierRange(doc, cnd->getName(), cnd->getSpan());
            sym.detail = (cnd->isExported() ? "export const " : "const ") + cnd->getName() + ": " + formatType(cnd->getType(), "any");
            symbols.push_back(std::move(sym));
        } else if (const auto* dd = dynamic_cast<const ast::DefineDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = dd->getName();
            sym.kind = SymbolKind::Constant;
            sym.range = spanToRange(doc, dd->getSpan());
            sym.selectionRange = findIdentifierRange(doc, dd->getName(), dd->getSpan());
            sym.detail = "define " + dd->getName();
            symbols.push_back(std::move(sym));
        } else if (const auto* errd = dynamic_cast<const ast::ErrTypeDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = errd->getName();
            sym.kind = SymbolKind::Struct;
            sym.range = spanToRange(doc, errd->getSpan());
            sym.selectionRange = findIdentifierRange(doc, errd->getName(), errd->getSpan());
            sym.detail = (errd->getBaseError().empty() ? "errtype" : "inherits " + errd->getBaseError());

            for (const auto& field : errd->getFields()) {
                DocumentSymbol fSym;
                fSym.name = field.name;
                fSym.kind = SymbolKind::Field;
                fSym.range = spanToRange(doc, field.span);
                fSym.selectionRange = findIdentifierRange(doc, field.name, field.span);
                fSym.detail = field.name + ": " + formatType(field.type.get(), "any");
                sym.children.push_back(std::move(fSym));
            }

            symbols.push_back(std::move(sym));
        } else if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
            DocumentSymbol sym;
            sym.name = !imp->getModuleAlias().empty() ? imp->getModuleAlias() : imp->getModulePath();
            sym.kind = SymbolKind::Module;
            sym.range = spanToRange(doc, imp->getSpan());
            sym.selectionRange = sym.range;
            sym.detail = "import \"" + imp->getModulePath() + "\"";
            symbols.push_back(std::move(sym));
        }
    }

    return symbols;
}

std::vector<TextEdit> Analyzer::format(const Document& doc, const FormattingOptions& options) {
    keety::fmt::FormatterOptions fmtOpts;
    if (options.tabSize > 0) {
        fmtOpts.indentWidth = options.tabSize;
    }

    keety::fmt::Formatter formatter(fmtOpts);
    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm);

    std::string formatted = formatter.format(doc.text(), doc.uri(), &emitter);
    if (emitter.hasErrors()) {
        // Do not return destructive edits if file has syntax errors
        return {};
    }

    if (formatted == doc.text()) {
        return {};
    }

    TextEdit edit;
    edit.range.start = Position{0, 0};
    edit.range.end = doc.offsetToPosition(doc.text().size());
    edit.newText = std::move(formatted);

    return {std::move(edit)};
}

std::vector<CodeAction> Analyzer::getCodeActions(
    const Document& doc,
    Range range,
    const std::vector<Diagnostic>& /*contextDiagnostics*/
) {
    std::vector<CodeAction> actions;

    diag::SourceManager sm;
    std::string pathStr = uriToPath(doc.uri());
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());

    lexer::Lexer lexer(fileId, doc.text());
    lexer.setCaptureComments(true);
    parser::Parser parser(lexer);

    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();
    auto ast = parser.parseModule(moduleName);
    auto comments = lexer.getCapturedComments();

    lint::LintConfig lintConfig = lint::LintConfig::discover(fsPath.parent_path());
    lint::LintEngine lintEngine(lintConfig);
    lintEngine.registerDefaultRules();

    auto lintDiags = lintEngine.lintModule(ast, comments, sm, doc.text(), pathStr);

    for (const auto& ld : lintDiags) {
        if (!ld.quickFix || !ld.quickFix->span.isValid()) {
            continue;
        }

        Range diagRange = spanToRange(doc, ld.span);
        Range fixRange = spanToRange(doc, ld.quickFix->span);

        // Check if the requested range overlaps with the diagnostic span or fix span
        bool overlaps = true;
        if (range.end.line < diagRange.start.line ||
            (range.end.line == diagRange.start.line && range.end.character < diagRange.start.character)) {
            overlaps = false;
        } else if (diagRange.end.line < range.start.line ||
                   (diagRange.end.line == range.start.line && diagRange.end.character < range.start.character)) {
            overlaps = false;
        }

        if (!overlaps) {
            bool fixOverlaps = true;
            if (range.end.line < fixRange.start.line ||
                (range.end.line == fixRange.start.line && range.end.character < fixRange.start.character)) {
                fixOverlaps = false;
            } else if (fixRange.end.line < range.start.line ||
                       (fixRange.end.line == range.start.line && fixRange.end.character < range.start.character)) {
                fixOverlaps = false;
            }
            if (!fixOverlaps) {
                continue;
            }
        }

        std::string title;
        if (ld.ruleCode == "L001") {
            title = "Remove unused import";
        } else if (ld.ruleCode == "L002") {
            title = "Prefix unused variable with '_'";
        } else if (ld.ruleCode == "L003") {
            title = "Prefix unused parameter with '_'";
        } else if (ld.ruleCode == "L010") {
            title = "Remove unnecessary 'mut'";
        } else {
            title = "Fix " + ld.ruleName + ": " + ld.message;
        }

        TextEdit edit;
        edit.range = fixRange;
        edit.newText = ld.quickFix->replacement;

        if (ld.ruleCode == "L001" && edit.newText.empty()) {
            size_t startOff = ld.quickFix->span.start.offset;
            size_t endOff = ld.quickFix->span.end.offset;
            const std::string& src = doc.text();
            if (endOff <= src.size() && startOff <= endOff) {
                size_t lineStart = startOff;
                while (lineStart > 0 && src[lineStart - 1] != '\n' && (src[lineStart - 1] == ' ' || src[lineStart - 1] == '\t')) {
                    --lineStart;
                }
                if (lineStart == 0 || src[lineStart - 1] == '\n') {
                    startOff = lineStart;
                }
                if (endOff < src.size() && src[endOff] == '\r' && endOff + 1 < src.size() && src[endOff + 1] == '\n') {
                    endOff += 2;
                } else if (endOff < src.size() && src[endOff] == '\n') {
                    endOff += 1;
                }
                edit.range.start = doc.offsetToPosition(startOff);
                edit.range.end = doc.offsetToPosition(endOff);
            }
        }

        CodeAction action;
        action.title = title;
        action.kind = "quickfix";
        action.isPreferred = true;

        WorkspaceEdit wsEdit;
        wsEdit.changes[doc.uri()] = { edit };
        action.edit = std::move(wsEdit);

        Diagnostic d;
        d.range = diagRange;
        d.severity = convertSeverity(ld.severity);
        d.code = ld.ruleCode;
        d.source = "keetlint";
        d.message = ld.message;
        action.diagnostics.push_back(std::move(d));

        actions.push_back(std::move(action));
    }

    return actions;
}

} // namespace keety::lsp
