#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#if defined(remove)
#undef remove
#endif

namespace keet {

/// Generic Result container for fallible operations supporting seamless unwrap/error inspection.
template <typename T>
class Result {
    std::variant<T, keet::error> data_;
public:
    Result(T val) : data_(std::move(val)) {}
    Result(keet::error err) : data_(std::move(err)) {}

    [[nodiscard]] bool is_ok() const noexcept { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool is_err() const noexcept { return std::holds_alternative<keet::error>(data_); }

    [[nodiscard]] const T& unwrap() const {
        if (is_err()) {
            throw std::get<keet::error>(data_);
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] const keet::error& error() const {
        if (is_ok()) {
            static const keet::error no_err("NoError", "Result holds success value");
            return no_err;
        }
        return std::get<keet::error>(data_);
    }

    operator T() const { return unwrap(); }
};

namespace fs {

/// Reads entire text file as UTF-8 string. Throws keet::error on failure.
inline std::string readText(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        throw keet::error("FileSystemError", "Cannot open file for reading: '" + path + "'");
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (ifs.bad()) {
        throw keet::error("FileSystemError", "I/O error while reading file: '" + path + "'");
    }
    return content;
}

/// Reads file lines into a vector of strings, stripping \r\n and \n. Throws keet::error on failure.
inline std::vector<std::string> readLines(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        throw keet::error("FileSystemError", "Cannot open file for reading lines: '" + path + "'");
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    if (ifs.bad()) {
        throw keet::error("FileSystemError", "I/O error while reading lines from file: '" + path + "'");
    }
    return lines;
}

/// Overwrites or creates file with text. Returns true on success; throws keet::error on failure.
inline bool writeText(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        throw keet::error("FileSystemError", "Cannot open file for writing: '" + path + "'");
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs.good()) {
        throw keet::error("FileSystemError", "Failed to write content to file: '" + path + "'");
    }
    ofs.flush();
    return true;
}

/// Appends text to an existing or new file. Returns true on success; throws keet::error on failure.
inline bool appendText(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!ofs.is_open()) {
        throw keet::error("FileSystemError", "Cannot open file for appending: '" + path + "'");
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs.good()) {
        throw keet::error("FileSystemError", "Failed to append content to file: '" + path + "'");
    }
    ofs.flush();
    return true;
}

/// Checks if path exists. Non-throwing.
[[nodiscard]] inline bool exists(const std::string& path) noexcept {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

/// Checks if path is a regular file. Non-throwing.
[[nodiscard]] inline bool isFile(const std::string& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

/// Checks if path is a directory. Non-throwing.
[[nodiscard]] inline bool isDir(const std::string& path) noexcept {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) && !ec;
}

/// Returns file size in bytes as int64_t. Throws keet::error on failure.
inline int64_t fileSize(const std::string& path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    if (ec) {
        throw keet::error("FileSystemError", "Cannot query file size of '" + path + "': " + ec.message());
    }
    return static_cast<int64_t>(sz);
}

/// Recursively creates directory and any necessary parents. Returns true on success; throws keet::error on failure.
inline bool mkdir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        throw keet::error("FileSystemError", "mkdir failed for '" + path + "': " + ec.message());
    }
    return true;
}

/// Removes a file or empty directory. Returns true if removed, false if target did not exist. Throws keet::error on failure.
inline bool remove(const std::string& path) {
    std::error_code ec;
    bool removed = std::filesystem::remove(path, ec);
    if (ec) {
        throw keet::error("FileSystemError", "remove failed for '" + path + "': " + ec.message());
    }
    return removed;
}

/// Recursively removes a directory/file and returns count of removed items. Throws keet::error on failure.
inline int64_t removeAll(const std::string& path) {
    std::error_code ec;
    auto count = std::filesystem::remove_all(path, ec);
    if (ec) {
        throw keet::error("FileSystemError", "remove_all failed for '" + path + "': " + ec.message());
    }
    return static_cast<int64_t>(count);
}

/// Copies file or directory recursively with overwrite option. Returns true on success; throws keet::error on failure.
inline bool copy(const std::string& src, const std::string& dst) {
    std::error_code ec;
    auto opts = std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive;
    std::filesystem::copy(src, dst, opts, ec);
    if (ec) {
        throw keet::error("FileSystemError", "copy failed from '" + src + "' to '" + dst + "': " + ec.message());
    }
    return true;
}

/// Moves or renames a file or directory. Returns true on success; throws keet::error on failure.
inline bool rename(const std::string& src, const std::string& dst) {
    std::error_code ec;
    std::filesystem::rename(src, dst, ec);
    if (ec) {
        throw keet::error("FileSystemError", "rename failed from '" + src + "' to '" + dst + "': " + ec.message());
    }
    return true;
}

/// Returns a sorted list of entry names inside a directory. Throws keet::error on failure.
inline std::vector<std::string> listDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::directory_iterator it(path, ec);
    if (ec) {
        throw keet::error("FileSystemError", "list_dir failed for '" + path + "': " + ec.message());
    }
    std::vector<std::string> entries;
    for (const auto& entry : it) {
        entries.push_back(entry.path().filename().generic_string());
    }
    std::sort(entries.begin(), entries.end());
    return entries;
}

/// Returns current working directory.
[[nodiscard]] inline std::string cwd() {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        return ".";
    }
    return p.generic_string();
}

/// Joins two path segments cleanly using platform-normalized separators.
[[nodiscard]] inline std::string join(const std::string& base, const std::string& child) {
    return (std::filesystem::path(base) / child).lexically_normal().generic_string();
}

/// Extracts file name with extension.
[[nodiscard]] inline std::string fileName(const std::string& path) {
    return std::filesystem::path(path).filename().generic_string();
}

/// Extracts file extension (including leading dot).
[[nodiscard]] inline std::string extension(const std::string& path) {
    return std::filesystem::path(path).extension().generic_string();
}

/// Extracts parent directory path.
[[nodiscard]] inline std::string parentDir(const std::string& path) {
    return std::filesystem::path(path).parent_path().generic_string();
}

} // namespace fs
} // namespace keet
