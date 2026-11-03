#include <keety/driver/project_root.hpp>

#include <fstream>
#include <system_error>
#include <keet/json.hpp>

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

std::optional<std::filesystem::path> resolveBuildDirectory(const std::filesystem::path& entryFile) {
    auto checkDirForBuildDir = [](const std::filesystem::path& start) -> std::optional<std::filesystem::path> {
        std::error_code ec;
        std::filesystem::path curr = std::filesystem::absolute(start, ec);
        if (ec) curr = start;

        while (!curr.empty()) {
            std::filesystem::path cand = curr / "pedigree.json";
            if (std::filesystem::is_regular_file(cand, ec)) {
                std::ifstream ifs(cand);
                if (ifs.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(ifs)),
                                         std::istreambuf_iterator<char>());
                    try {
                        keet::json::detail::Parser parser(content);
                        auto rootNode = parser.parse_document();
                        if (rootNode.is_object()) {
                            const auto& obj = std::get<keet::json::detail::JsonNode::Object>(rootNode.value);
                            auto it = obj.find("buildDir");
                            if (it != obj.end() && it->second.is_string()) {
                                std::string bDirStr = std::get<std::string>(it->second.value);
                                if (!bDirStr.empty()) {
                                    std::filesystem::path bPath(bDirStr);
                                    if (bPath.is_absolute()) {
                                        return bPath.lexically_normal();
                                    } else {
                                        return (curr / bPath).lexically_normal();
                                    }
                                }
                            }
                        }
                    } catch (...) {
                    }
                }
            }
            std::filesystem::path parent = curr.parent_path();
            if (parent == curr) break;
            curr = parent;
        }
        return std::nullopt;
    };

    // 1. Walk upward from current working directory
    if (auto res = checkDirForBuildDir(std::filesystem::current_path())) {
        return res;
    }

    // 2. Walk upward from entry file's parent directory if provided
    if (!entryFile.empty()) {
        std::filesystem::path entryParent = entryFile.has_parent_path()
            ? entryFile.parent_path()
            : std::filesystem::current_path();
        if (auto res = checkDirForBuildDir(entryParent)) {
            return res;
        }
    }

    return std::nullopt;
}

} // namespace keety::driver

