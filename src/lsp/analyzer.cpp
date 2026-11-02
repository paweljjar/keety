#include <keety/lsp/analyzer.hpp>

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/driver/project_root.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>
#include <keety/resolver/module_path_resolver.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/universe.hpp>
#include <keety/typechecker/type_checker.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_map>

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
        {"println", {"fn println(...args: any): void", "Prints arguments to standard output followed by a newline."}},
        {"print", {"fn print(...args: any): void", "Prints arguments to standard output without a trailing newline."}},
        {"eprintln", {"fn eprintln(...args: any): void", "Prints arguments to standard error followed by a newline."}},
        {"eprint", {"fn eprint(...args: any): void", "Prints arguments to standard error without a trailing newline."}},
        {"readln", {"fn readln(): string", "Reads a line of text from standard input (without newline)."}},
        {"readLine", {"fn readLine(): string", "Reads a line of text from standard input (without newline)."}},
        {"input", {"fn input(prompt: string): string", "Displays a prompt and reads a line of user input."}},
        {"flush", {"fn flush(): void", "Flushes standard output buffer."}},
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

} // namespace

std::vector<Diagnostic> Analyzer::analyze(const Document& doc) {
    std::vector<Diagnostic> lspDiagnostics;

    diag::SourceManager sm;
    diag::DiagnosticEmitter emitter(sm, diag::ColorMode::Never);

    std::string pathStr = uriToPath(doc.uri());
    diag::FileId fileId = sm.addBuffer(pathStr, doc.text());

    // 1. Lexical & Syntactic Analysis (Parser)
    lexer::Lexer lexer(fileId, doc.text());
    parser::Parser parser(lexer, &emitter);

    std::filesystem::path fsPath(pathStr);
    std::string moduleName = fsPath.stem().string();
    auto ast = parser.parseModule(moduleName);

    if (emitter.hasErrors()) {
        collectLspDiagnostics(doc, emitter.getDiagnostics(), lspDiagnostics);
        return lspDiagnostics;
    }

    // 2. Resolve Module Dependencies & Pre-populate Scopes
    auto universe = std::make_shared<sema::Scope>(sema::ScopeKind::Universe);
    sema::registerUniverseSymbols(*universe);

    auto moduleScope = std::make_shared<sema::Scope>(sema::ScopeKind::Module, universe);

    // Register standard library modules in scope
    diag::SourceSpan dummySpan{};
    moduleScope->define(std::make_shared<sema::Symbol>("io", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("fs", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("sys", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("math", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("time", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("json", sema::SymbolKind::Module, dummySpan), false);
    moduleScope->define(std::make_shared<sema::Symbol>("net", sema::SymbolKind::Module, dummySpan), false);

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
                    moduleScope->define(
                        std::make_shared<sema::Symbol>(effName, sema::SymbolKind::Module, imp->getSpan()),
                        false
                    );
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
            if (modAlias == ident || modPath == ident) {
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

    // 3b. Standard Library Built-in Functions/Constants
    if (auto stdDoc = getStdlibMemberDoc(ident); stdDoc.has_value()) {
        std::string content = "```keet\n" + stdDoc->signature + "\n```\n" + stdDoc->doc;
        return Hover{MarkupContent{"markdown", std::move(content)}, tokRange};
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

    return std::nullopt;
}

} // namespace keety::lsp
