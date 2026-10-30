#pragma once

#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/error_code.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace keety::resolver {

/// Classification of resolved module source origin
enum class ModuleKind {
    StandardLibrary, // Built-in KeetScript stdlib module (e.g. `io`, `math`, `sys`)
    Local,           // Local project module (e.g. `./utils/helpers.keet`)
    Package,         // External third-party package inside `keet_modules/`
    NativeHeader     // C/C++ native header (e.g. `./native/fast_hash.hpp`)
};

/// Represents a successfully resolved module
struct ResolvedModule {
    ModuleKind kind{ModuleKind::Local};
    std::string moduleName;                  // Canonical module name or alias
    std::filesystem::path canonicalPath;     // Absolute canonical path on disk (or virtual stdlib path)
    bool isNativeHeader{false};

    [[nodiscard]] bool isStandardLibrary() const noexcept {
        return kind == ModuleKind::StandardLibrary;
    }

    [[nodiscard]] bool isLocal() const noexcept {
        return kind == ModuleKind::Local;
    }

    [[nodiscard]] bool isPackage() const noexcept {
        return kind == ModuleKind::Package;
    }
};

/// Configurable module path resolver conforming to Keety Compiler Architecture Specification (Stage 3).
/// Resolves standard library modules, third-party `keet_modules/`, and local `./relative` paths.
class ModulePathResolver {
public:
    explicit ModulePathResolver(std::filesystem::path projectRoot = std::filesystem::current_path(),
                                diag::DiagnosticEmitter* emitter = nullptr);

    /// Sets an explicit directory containing standard library sources (if available)
    void setStdlibDirectory(std::filesystem::path stdlibDir) {
        stdlibDir_ = std::move(stdlibDir);
    }

    /// Sets an explicit directory for third-party packages (defaults to `<projectRoot>/keet_modules`)
    void setKeetModulesDirectory(std::filesystem::path keetModulesDir) {
        keetModulesDir_ = std::move(keetModulesDir);
    }

    /// Returns true if the given unquoted qualified identifier path is a recognized standard library module
    [[nodiscard]] static bool isStandardLibraryModule(std::string_view importPath) noexcept;

    /// Resolves an import path relative to the importing file's location.
    /// Emits E1002 (ModuleNotFound) or E1003 (CannotImportSelf) if resolution fails and an emitter is set.
    [[nodiscard]] std::optional<ResolvedModule> resolve(
        const std::filesystem::path& importingFilePath,
        const std::string& importPath,
        diag::SourceSpan importSpan = diag::SourceSpan{});

    [[nodiscard]] const std::filesystem::path& getProjectRoot() const noexcept {
        return projectRoot_;
    }

    [[nodiscard]] const std::filesystem::path& getKeetModulesDirectory() const noexcept {
        return keetModulesDir_;
    }

private:
    std::filesystem::path projectRoot_;
    std::filesystem::path keetModulesDir_;
    std::optional<std::filesystem::path> stdlibDir_;
    diag::DiagnosticEmitter* emitter_{nullptr};

    std::optional<ResolvedModule> resolveLocalImport(
        const std::filesystem::path& importingFilePath,
        const std::string& importPath,
        diag::SourceSpan importSpan);

    std::optional<ResolvedModule> resolveQualifiedImport(
        const std::filesystem::path& importingFilePath,
        const std::string& importPath,
        diag::SourceSpan importSpan);
};

} // namespace keety::resolver
