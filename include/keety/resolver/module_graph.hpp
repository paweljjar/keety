#pragma once

#include <keety/ast/decl.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/parser/parser.hpp>
#include <keety/resolver/module_path_resolver.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace keety::resolver {

/// Directed dependency edge (u -> v): module `u` imports module `v`
struct DependencyEdge {
    std::string fromCanonicalPath;
    std::string toCanonicalPath;
    diag::SourceSpan importSpan{};
    std::string rawImportPath;
};

/// Vertex in the module dependency graph representing a single autonomous module
struct ModuleNode {
    std::string canonicalPath;                       // Unique canonical path or virtual stdlib ID
    std::string moduleName;                          // Module identifier or base name
    diag::FileId fileId{diag::INVALID_FILE_ID};      // SourceManager FileId (if loaded)
    std::shared_ptr<parser::ModuleAST> ast{nullptr}; // Cached parsed AST (single-instantiation deduplication)
    ModuleKind kind{ModuleKind::Local};
    bool isNativeHeader{false};
    bool isKmlComponent{false};
    std::string kmlGeneratedHpp;
    std::string kmlGeneratedCpp;

    std::vector<DependencyEdge> outgoingEdges;       // Modules this module depends upon (u -> v)
};

/// Directed Module Dependency Graph G = (V, E) conforming to
/// Keety Compiler Architecture Specification (Stage 3).
class ModuleGraph {
public:
    ModuleGraph() = default;

    /// Adds or retrieves a module node in the graph
    ModuleNode& getOrCreateNode(const std::string& canonicalPath,
                                const std::string& moduleName = "",
                                ModuleKind kind = ModuleKind::Local,
                                bool isNativeHeader = false);

    /// Adds a directed dependency edge: `fromPath` imports `toPath`
    void addEdge(const std::string& fromPath,
                 const std::string& toPath,
                 diag::SourceSpan importSpan = diag::SourceSpan{},
                 std::string rawImportPath = "");

    /// Checks if a module node exists in the graph
    [[nodiscard]] bool hasNode(const std::string& canonicalPath) const noexcept;

    /// Retrieves a module node, or nullptr if not found
    [[nodiscard]] const ModuleNode* getNode(const std::string& canonicalPath) const noexcept;
    [[nodiscard]] ModuleNode* getNode(const std::string& canonicalPath) noexcept;

    /// Returns all module nodes in the graph
    [[nodiscard]] const std::unordered_map<std::string, ModuleNode>& getNodes() const noexcept {
        return nodes_;
    }

    /// Returns module canonical paths in order of insertion / discovery
    [[nodiscard]] const std::vector<std::string>& getInsertionOrder() const noexcept {
        return insertionOrder_;
    }

    /// Returns the number of vertices |V|
    [[nodiscard]] size_t vertexCount() const noexcept { return nodes_.size(); }

    /// Returns the total number of directed edges |E|
    [[nodiscard]] size_t edgeCount() const noexcept;

    /// Recursively builds the module graph starting from an entry `.keet` file
    /// using the provided SourceManager, Parser, and ModulePathResolver.
    /// Deduplicates parsed modules so each module file is loaded and parsed exactly once.
    bool buildGraph(const std::filesystem::path& entryFilePath,
                    diag::SourceManager& sourceManager,
                    ModulePathResolver& pathResolver,
                    diag::DiagnosticEmitter* emitter = nullptr);

private:
    std::unordered_map<std::string, ModuleNode> nodes_;
    std::vector<std::string> insertionOrder_;
};

} // namespace keety::resolver
