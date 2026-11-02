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
        if (targetOffset >= tok.span.start.offset && targetOffset <= tok.span.end.offset) {
            targetTok = &tok;
            break;
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

} // namespace keety::lsp
