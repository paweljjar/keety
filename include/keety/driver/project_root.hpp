#pragma once

#include <filesystem>

namespace keety::driver {

/// Determines the project root directory for an entry file:
/// 1. Nearest ancestor directory containing `keet_modules/`
/// 2. Nearest ancestor directory containing `pedigree.json`
/// 3. The entry file's parent directory (or current_path() if empty)
[[nodiscard]] std::filesystem::path findProjectRoot(
    const std::filesystem::path& entryFile
);

} // namespace keety::driver
