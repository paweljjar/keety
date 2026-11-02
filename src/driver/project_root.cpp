#include <keety/driver/project_root.hpp>

#include <system_error>

namespace keety::driver {

std::filesystem::path findProjectRoot(const std::filesystem::path& entryFile) {
    std::filesystem::path fallback = entryFile.has_parent_path()
        ? entryFile.parent_path()
        : std::filesystem::current_path();

    std::error_code ec;
    std::filesystem::path start = std::filesystem::absolute(fallback, ec);
    if (ec) {
        start = fallback;
    }

    // 1. Walk upward looking for an ancestor containing `keet_modules/`
    std::filesystem::path curr = start;
    while (!curr.empty()) {
        std::filesystem::path cand = curr / "keet_modules";
        if (std::filesystem::is_directory(cand, ec)) {
            return curr;
        }
        std::filesystem::path parent = curr.parent_path();
        if (parent == curr) break;
        curr = parent;
    }

    // 2. Walk upward looking for an ancestor containing `pedigree.json`
    curr = start;
    while (!curr.empty()) {
        std::filesystem::path cand = curr / "pedigree.json";
        if (std::filesystem::is_regular_file(cand, ec)) {
            return curr;
        }
        std::filesystem::path parent = curr.parent_path();
        if (parent == curr) break;
        curr = parent;
    }

    // 3. Fallback to entry file's parent directory
    return fallback;
}

} // namespace keety::driver
