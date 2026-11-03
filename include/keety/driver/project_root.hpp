#pragma once

#include <filesystem>
#include <optional>

namespace keety::driver {

/// Determines the project root directory for an entry file:
/// 1. Nearest ancestor directory containing `keet_modules/`
/// 2. Nearest ancestor directory containing `pedigree.json`
/// 3. The entry file's parent directory (or current_path() if empty)
[[nodiscard]] std::filesystem::path findProjectRoot(
    const std::filesystem::path& entryFile
);

/// Discovers configured build directory from pedigree.json's "buildDir" property,
/// searching from entry file's directory and current working directory upwards.
/// Returns std::nullopt if no pedigree.json is found or if "buildDir" is not specified.
[[nodiscard]] std::optional<std::filesystem::path> resolveBuildDirectory(
    const std::filesystem::path& entryFile = {}
);

} // namespace keety::driver
