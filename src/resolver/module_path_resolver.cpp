#include <keety/resolver/module_path_resolver.hpp>

#include <algorithm>
#include <system_error>

namespace keety::resolver {

ModulePathResolver::ModulePathResolver(std::filesystem::path projectRoot, diag::DiagnosticEmitter* emitter)
    : projectRoot_(std::move(projectRoot)),
      keetModulesDir_(projectRoot_ / "keet_modules"),
      emitter_(emitter) {}

bool ModulePathResolver::isStandardLibraryModule(std::string_view importPath) noexcept {
    static const std::unordered_set<std::string_view> stdlibModules = {
        "io",
        "fs",
        "sys",
        "math",
        "time",
        "json",
        "net",
        "net.http",
        "net.tcp",
        "net.udp",
        "http",
        "crypto",
        "crypto.hash",
        "encoding",
        "encoding.json",
        "encoding.csv",
        "test",
        "ui",
        "os"
    };

    if (stdlibModules.find(importPath) != stdlibModules.end()) {
        return true;
    }

    return false;
}

std::optional<ResolvedModule> ModulePathResolver::resolve(
    const std::filesystem::path& importingFilePath,
    const std::string& importPath,
    diag::SourceSpan importSpan) {

    // Relative path or quoted file path starting with './' or '../'
    bool isRelative = (importPath.rfind("./", 0) == 0) ||
                      (importPath.rfind("../", 0) == 0) ||
                      (importPath.rfind(".\\", 0) == 0) ||
                      (importPath.rfind("..\\", 0) == 0) ||
                      (importPath.find('/') != std::string::npos) ||
                      (importPath.find('\\') != std::string::npos);

    if (isRelative) {
        return resolveLocalImport(importingFilePath, importPath, importSpan);
    }

    return resolveQualifiedImport(importingFilePath, importPath, importSpan);
}

std::optional<ResolvedModule> ModulePathResolver::resolveLocalImport(
    const std::filesystem::path& importingFilePath,
    const std::string& importPath,
    diag::SourceSpan importSpan) {

    std::filesystem::path baseDir = importingFilePath.has_parent_path()
                                        ? importingFilePath.parent_path()
                                        : projectRoot_;

    std::filesystem::path rawTarget = baseDir / importPath;
    std::error_code ec;

    // 1. Check for native C/C++ headers (.h, .hpp)
    std::string ext = rawTarget.extension().string();
    if (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".hh") {
        if (std::filesystem::exists(rawTarget, ec) && std::filesystem::is_regular_file(rawTarget, ec)) {
            // Check self-import
            if (std::filesystem::exists(importingFilePath, ec) &&
                std::filesystem::equivalent(rawTarget, importingFilePath, ec)) {
                if (emitter_) {
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::CannotImportSelf,
                        "module cannot import itself: '" + importingFilePath.string() + "'",
                        importSpan.start).withLabel(importSpan, "cannot import self"));
                }
                return std::nullopt;
            }

            std::filesystem::path canonical = std::filesystem::canonical(rawTarget, ec);
            if (ec) {
                canonical = std::filesystem::weakly_canonical(rawTarget);
            }
            return ResolvedModule{ModuleKind::NativeHeader, rawTarget.stem().string(), canonical, true};
        }
    }

    // 2. Check for .keet or .kml module candidates
    std::filesystem::path candidate;

    if (ext == ".keet" || ext == ".kml") {
        if (std::filesystem::exists(rawTarget, ec) && std::filesystem::is_regular_file(rawTarget, ec)) {
            candidate = rawTarget;
        }
    } else {
        // Try appending .keet, then .kml
        std::filesystem::path withExt = rawTarget;
        withExt += ".keet";
        if (std::filesystem::exists(withExt, ec) && std::filesystem::is_regular_file(withExt, ec)) {
            candidate = withExt;
        } else {
            std::filesystem::path withKml = rawTarget;
            withKml += ".kml";
            if (std::filesystem::exists(withKml, ec) && std::filesystem::is_regular_file(withKml, ec)) {
                candidate = withKml;
            } else {
                // Try directory with index.keet or index.kml
                std::filesystem::path withIndex = rawTarget / "index.keet";
                if (std::filesystem::exists(withIndex, ec) && std::filesystem::is_regular_file(withIndex, ec)) {
                    candidate = withIndex;
                } else {
                    std::filesystem::path withIndexKml = rawTarget / "index.kml";
                    if (std::filesystem::exists(withIndexKml, ec) && std::filesystem::is_regular_file(withIndexKml, ec)) {
                        candidate = withIndexKml;
                    }
                }
            }
        }
    }

    if (candidate.empty()) {
        if (emitter_) {
            emitter_->report(diag::Diagnostic::makeError(
                diag::ErrorCode::ModuleNotFound,
                "could not resolve local module '" + importPath + "' from '" + importingFilePath.string() + "'",
                importSpan.start).withLabel(importSpan, "module not found"));
        }
        return std::nullopt;
    }

    // Check self-import (CannotImportSelf - E1003)
    if (std::filesystem::exists(importingFilePath, ec) &&
        std::filesystem::equivalent(candidate, importingFilePath, ec)) {
        if (emitter_) {
            emitter_->report(diag::Diagnostic::makeError(
                diag::ErrorCode::CannotImportSelf,
                "module cannot import itself: '" + importingFilePath.string() + "'",
                importSpan.start).withLabel(importSpan, "cannot import self"));
        }
        return std::nullopt;
    }

    std::filesystem::path canonical = std::filesystem::canonical(candidate, ec);
    if (ec) {
        canonical = std::filesystem::weakly_canonical(candidate);
    }

    std::string modName = ((canonical.filename() == "index.keet" || canonical.filename() == "index.kml") && canonical.parent_path() != baseDir)
                              ? canonical.parent_path().filename().string()
                              : canonical.stem().string();

    return ResolvedModule{ModuleKind::Local, std::move(modName), canonical, false};
}

std::optional<ResolvedModule> ModulePathResolver::resolveQualifiedImport(
    const std::filesystem::path& importingFilePath,
    const std::string& importPath,
    diag::SourceSpan importSpan) {

    std::error_code ec;

    // 1. Check Standard Library
    if (isStandardLibraryModule(importPath)) {
        if (stdlibDir_.has_value()) {
            std::string slashPath = importPath;
            std::replace(slashPath.begin(), slashPath.end(), '.', '/');
            std::filesystem::path cand = *stdlibDir_ / (slashPath + ".keet");
            if (std::filesystem::exists(cand, ec) && std::filesystem::is_regular_file(cand, ec)) {
                std::filesystem::path canonical = std::filesystem::canonical(cand, ec);
                return ResolvedModule{ModuleKind::StandardLibrary, importPath, canonical, false};
            }
        }

        // Built-in standard library module with virtual canonical path
        return ResolvedModule{
            ModuleKind::StandardLibrary,
            importPath,
            std::filesystem::path("<stdlib>") / importPath,
            false
        };
    }

    // 2. Check third-party `keet_modules/`
    std::string slashPath = importPath;
    std::replace(slashPath.begin(), slashPath.end(), '.', '/');

    std::string hyphenPath = importPath;
    std::replace(hyphenPath.begin(), hyphenPath.end(), '_', '-');

    std::vector<std::filesystem::path> candidates = {
        keetModulesDir_ / (slashPath + ".keet"),
        keetModulesDir_ / (slashPath + ".kml"),
        keetModulesDir_ / slashPath / "index.keet",
        keetModulesDir_ / slashPath / "index.kml",
        keetModulesDir_ / (importPath + ".keet"),
        keetModulesDir_ / (importPath + ".kml"),
        keetModulesDir_ / importPath / "index.keet",
        keetModulesDir_ / importPath / "index.kml",
        keetModulesDir_ / (hyphenPath + ".keet"),
        keetModulesDir_ / (hyphenPath + ".kml"),
        keetModulesDir_ / hyphenPath / "index.keet",
        keetModulesDir_ / hyphenPath / "index.kml"
    };

    for (const auto& cand : candidates) {
        if (std::filesystem::exists(cand, ec) && std::filesystem::is_regular_file(cand, ec)) {
            // Check self-import
            if (std::filesystem::exists(importingFilePath, ec) &&
                std::filesystem::equivalent(cand, importingFilePath, ec)) {
                if (emitter_) {
                    emitter_->report(diag::Diagnostic::makeError(
                        diag::ErrorCode::CannotImportSelf,
                        "module cannot import itself: '" + importingFilePath.string() + "'",
                        importSpan.start).withLabel(importSpan, "cannot import self"));
                }
                return std::nullopt;
            }

            std::filesystem::path canonical = std::filesystem::canonical(cand, ec);
            if (ec) {
                canonical = std::filesystem::weakly_canonical(cand);
            }

            std::string dirName = cand.parent_path().filename().string();
            std::string modName;
            if (cand.filename() == "index.keet" || cand.filename() == "index.kml") {
                if (dirName == hyphenPath) {
                    modName = importPath;
                } else {
                    modName = dirName;
                }
            } else {
                modName = cand.stem().string();
            }

            return ResolvedModule{ModuleKind::Package, std::move(modName), canonical, false};
        }
    }

    // 3. Not found in standard library or keet_modules/
    if (emitter_) {
        emitter_->report(diag::Diagnostic::makeError(
            diag::ErrorCode::ModuleNotFound,
            "module '" + importPath + "' not found in standard library or keet_modules/",
            importSpan.start).withLabel(importSpan, "module not found"));
    }

    return std::nullopt;
}

} // namespace keety::resolver
