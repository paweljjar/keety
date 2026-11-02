#include <keety/driver/toolchain_detector.hpp>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <regex>
#include <sstream>

namespace keety::driver {

namespace {

/// Helper to execute a command synchronously and capture stdout
std::string run_process_capture(const std::string& cmd) {
    std::string result;
    std::array<char, 256> buffer;
#if defined(_WIN32)
    std::string full_cmd = "\"" + cmd + "\"";
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(full_cmd.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
#endif
    if (!pipe) {
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

} // namespace

std::string_view to_string(CompilerKind kind) noexcept {
    switch (kind) {
    case CompilerKind::Clang:   return "Clang";
    case CompilerKind::GCC:     return "GCC";
    case CompilerKind::MSVC:    return "MSVC";
    case CompilerKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

CompilerKind ToolchainDetector::classify_from_path(const std::filesystem::path& path) {
    std::string path_str = path.string();
    auto last_slash = path_str.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? path_str.substr(last_slash + 1) : path_str;
    filename = to_lower(filename);
    if (filename.find("clang") != std::string::npos) {
        return CompilerKind::Clang;
    }
    if (filename.find("g++") != std::string::npos || filename.find("gcc") != std::string::npos) {
        return CompilerKind::GCC;
    }
    if (filename == "cl.exe" || filename == "cl") {
        return CompilerKind::MSVC;
    }
    return CompilerKind::Unknown;
}

CompilerKind ToolchainDetector::classify_from_banner(std::string_view banner) {
    std::string lower = to_lower(banner);
    if (lower.find("clang") != std::string::npos || lower.find("apple clang") != std::string::npos) {
        return CompilerKind::Clang;
    }
    if (lower.find("g++") != std::string::npos || lower.find("gcc") != std::string::npos || lower.find("free software foundation") != std::string::npos) {
        return CompilerKind::GCC;
    }
    if (lower.find("microsoft (r) c/c++") != std::string::npos || lower.find("optimizing compiler") != std::string::npos) {
        return CompilerKind::MSVC;
    }
    return CompilerKind::Unknown;
}

bool ToolchainDetector::parse_version_string(std::string_view version_str,
                                             int& major, int& minor, int& patch) {
    major = 0;
    minor = 0;
    patch = 0;

    // Matches e.g. "14.0.3", "21.0.0", "19.29.30133"
    std::regex ver_regex(R"((\d+)\.(\d+)(?:\.(\d+))?)");
    std::string s(version_str);
    std::smatch match;
    if (std::regex_search(s, match, ver_regex)) {
        if (match.size() > 1 && match[1].matched) {
            major = std::stoi(match[1].str());
        }
        if (match.size() > 2 && match[2].matched) {
            minor = std::stoi(match[2].str());
        }
        if (match.size() > 3 && match[3].matched) {
            patch = std::stoi(match[3].str());
        }
        return true;
    }
    return false;
}

std::optional<std::filesystem::path> ToolchainDetector::find_in_path(std::string_view command_name) {
    std::filesystem::path p(command_name);
    if (p.is_absolute() && std::filesystem::exists(p)) {
        return p;
    }

    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        return std::nullopt;
    }

#if defined(_WIN32)
    char delimiter = ';';
#else
    char delimiter = ':';
#endif

    std::string path_str(path_env);
    std::stringstream ss(path_str);
    std::string dir;
    while (std::getline(ss, dir, delimiter)) {
        if (dir.empty()) continue;
        std::filesystem::path candidate = std::filesystem::path(dir) / command_name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
#if defined(_WIN32)
        std::filesystem::path candidate_exe = std::filesystem::path(dir) / (std::string(command_name) + ".exe");
        if (std::filesystem::exists(candidate_exe)) {
            return candidate_exe;
        }
#endif
    }

    return std::nullopt;
}

std::vector<std::string> ToolchainDetector::get_candidate_names() {
    std::vector<std::string> candidates;
    // Check environment variable CXX first
    const char* cxx_env = std::getenv("CXX");
    if (cxx_env && *cxx_env) {
        candidates.emplace_back(cxx_env);
    }

#if defined(_WIN32)
    candidates.push_back("cl.exe");
    candidates.push_back("clang++");
    candidates.push_back("g++");
#elif defined(__APPLE__)
    candidates.push_back("clang++");
    candidates.push_back("g++");
#else
    candidates.push_back("g++");
    candidates.push_back("clang++");
#endif

    return candidates;
}

std::optional<ToolchainInfo> ToolchainDetector::probe(const std::filesystem::path& compiler_path_or_cmd) {
    std::optional<std::filesystem::path> resolved = find_in_path(compiler_path_or_cmd.string());
    if (!resolved) {
        if (std::filesystem::exists(compiler_path_or_cmd)) {
            resolved = compiler_path_or_cmd;
        } else {
            return std::nullopt;
        }
    }

    ToolchainInfo info;
    info.compiler_path = *resolved;
    info.kind = classify_from_path(*resolved);

    // Run --version or /? for MSVC to get banner
    std::string quote = "\"";
    std::string version_cmd = quote + resolved->string() + quote + " --version 2>&1";
    std::string output = run_process_capture(version_cmd);

    if (output.empty() && info.kind == CompilerKind::MSVC) {
        // MSVC uses /? or outputs banner without arguments
        version_cmd = quote + resolved->string() + quote + " 2>&1";
        output = run_process_capture(version_cmd);
    }

    if (output.empty()) {
        return std::nullopt;
    }

    // Banner classification refinement
    CompilerKind banner_kind = classify_from_banner(output);
    if (banner_kind != CompilerKind::Unknown) {
        info.kind = banner_kind;
    }

    info.version = output;
    (void)parse_version_string(output, info.major_version, info.minor_version, info.patch_version);

    // Extract Target triple if available (Clang / GCC: "Target: ...")
    std::regex target_regex(R"(Target:\s*([^\r\n]+))");
    std::smatch t_match;
    if (std::regex_search(output, t_match, target_regex)) {
        if (t_match.size() > 1) {
            info.target_triple = t_match[1].str();
        }
    }

    // Determine C++17 support:
    // Clang >= 5, GCC >= 7, MSVC >= 19.14 (VS 2017 15.7) or Apple Clang >= 10
    switch (info.kind) {
    case CompilerKind::Clang:
        info.supports_cpp17 = (info.major_version >= 5);
        break;
    case CompilerKind::GCC:
        info.supports_cpp17 = (info.major_version >= 7);
        break;
    case CompilerKind::MSVC:
        info.supports_cpp17 = (info.major_version >= 19);
        break;
    case CompilerKind::Unknown:
        info.supports_cpp17 = false;
        break;
    }

    return info;
}

std::optional<ToolchainInfo> ToolchainDetector::detect_default() {
    auto candidates = get_candidate_names();
    for (const auto& cand : candidates) {
        auto info = probe(cand);
        if (info && info->is_valid()) {
            return info;
        }
    }
    return std::nullopt;
}

} // namespace keety::driver
