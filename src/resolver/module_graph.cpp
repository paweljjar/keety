#include <keety/resolver/module_graph.hpp>

#include <fstream>
#include <sstream>
#include <system_error>

namespace keety::resolver {

ModuleNode& ModuleGraph::getOrCreateNode(const std::string& canonicalPath,
                                         const std::string& moduleName,
                                         ModuleKind kind,
                                         bool isNativeHeader) {
    auto it = nodes_.find(canonicalPath);
    if (it != nodes_.end()) {
        if (!moduleName.empty() && it->second.moduleName.empty()) {
            it->second.moduleName = moduleName;
        }
        return it->second;
    }

    ModuleNode node;
    node.canonicalPath = canonicalPath;
    node.moduleName = moduleName;
    node.kind = kind;
    node.isNativeHeader = isNativeHeader;

    nodes_.emplace(canonicalPath, std::move(node));
    insertionOrder_.push_back(canonicalPath);

    return nodes_[canonicalPath];
}

void ModuleGraph::addEdge(const std::string& fromPath,
                          const std::string& toPath,
                          diag::SourceSpan importSpan,
                          std::string rawImportPath) {
    auto& fromNode = getOrCreateNode(fromPath);
    getOrCreateNode(toPath);

    // Prevent duplicate edges
    for (const auto& edge : fromNode.outgoingEdges) {
        if (edge.toCanonicalPath == toPath) {
            return;
        }
    }

    fromNode.outgoingEdges.push_back(DependencyEdge{
        fromPath,
        toPath,
        importSpan,
        std::move(rawImportPath)
    });
}

bool ModuleGraph::hasNode(const std::string& canonicalPath) const noexcept {
    return nodes_.find(canonicalPath) != nodes_.end();
}

const ModuleNode* ModuleGraph::getNode(const std::string& canonicalPath) const noexcept {
    auto it = nodes_.find(canonicalPath);
    if (it != nodes_.end()) {
        return &it->second;
    }
    return nullptr;
}

ModuleNode* ModuleGraph::getNode(const std::string& canonicalPath) noexcept {
    auto it = nodes_.find(canonicalPath);
    if (it != nodes_.end()) {
        return &it->second;
    }
    return nullptr;
}

size_t ModuleGraph::edgeCount() const noexcept {
    size_t count = 0;
    for (const auto& [_, node] : nodes_) {
        count += node.outgoingEdges.size();
    }
    return count;
}

bool ModuleGraph::buildGraph(const std::filesystem::path& entryFilePath,
                             diag::SourceManager& sourceManager,
                             ModulePathResolver& pathResolver,
                             diag::DiagnosticEmitter* emitter) {
    std::error_code ec;
    std::filesystem::path canonicalEntry = std::filesystem::canonical(entryFilePath, ec);
    if (ec) {
        canonicalEntry = std::filesystem::weakly_canonical(entryFilePath);
    }

    std::string canonicalStr = canonicalEntry.string();

    // If already in graph and AST parsed, avoid redundant work (single instantiation)
    auto* existing = getNode(canonicalStr);
    if (existing && existing->ast != nullptr) {
        return true;
    }

    std::string modName = canonicalEntry.stem().string();
    auto& node = getOrCreateNode(canonicalStr, modName, ModuleKind::Local, false);

    // Read source file content
    std::ifstream file(canonicalEntry);
    if (!file.is_open()) {
        if (emitter) {
            emitter->report(diag::Diagnostic::makeError(
                diag::ErrorCode::ModuleNotFound,
                "could not open file '" + canonicalStr + "'",
                diag::SourceLocation{}));
        }
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    diag::FileId fileId = sourceManager.addBuffer(canonicalStr, content);
    node.fileId = fileId;

    lexer::Lexer lexer(fileId, content);
    parser::Parser parser(lexer, emitter);
    auto parsedModule = parser.parseModule(modName);

    node.ast = std::make_shared<parser::ModuleAST>(std::move(parsedModule));

    // Inspect import declarations to construct directed edges
    for (const auto& decl : node.ast->declarations) {
        auto* importDecl = dynamic_cast<const ast::ImportDecl*>(decl.get());
        if (!importDecl) {
            continue;
        }

        const std::string& importPath = importDecl->getModulePath();
        auto resolved = pathResolver.resolve(canonicalEntry, importPath, importDecl->getSpan());
        if (!resolved.has_value()) {
            continue;
        }

        std::string targetCanonical = (resolved->kind == ModuleKind::StandardLibrary && ! std::filesystem::exists(resolved->canonicalPath))
                                          ? resolved->canonicalPath.generic_string()
                                          : resolved->canonicalPath.string();
        addEdge(canonicalStr, targetCanonical, importDecl->getSpan(), importPath);

        auto& targetNode = getOrCreateNode(targetCanonical, resolved->moduleName, resolved->kind, resolved->isNativeHeader);

        // Recursively build graph for local and package .keet modules
        if ((resolved->kind == ModuleKind::Local || resolved->kind == ModuleKind::Package) &&
            !resolved->isNativeHeader && targetNode.ast == nullptr) {
            buildGraph(resolved->canonicalPath, sourceManager, pathResolver, emitter);
        }
    }

    return true;
}

} // namespace keety::resolver
