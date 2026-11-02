#include <keety/pkg/fetcher.hpp>
#include <keety/pkg/diagnostics.hpp>
#include <keety/pkg/process.hpp>

#include <system_error>

namespace keety::pkg {

LinkMethod Fetcher::linkOrCopy(
    const std::filesystem::path& target,
    const std::filesystem::path& dest
) {
    std::error_code ec;

    // Remove existing destination if any
    remove(dest);
    std::filesystem::create_directories(dest.parent_path(), ec);

    // 1. Attempt directory symlink
    std::filesystem::create_directory_symlink(target, dest, ec);
    if (!ec) {
        return LinkMethod::Symlink;
    }

#if defined(_WIN32)
    // 2. On Windows, try directory junction (mklink /J dest target)
    ec.clear();
    auto res = ProcessRunner::run({"cmd", "/C", "mklink", "/J", dest.string(), target.string()});
    if (res.success()) {
        return LinkMethod::Junction;
    }
#endif

    // 3. Fallback to recursive directory copy
    ec.clear();
    std::filesystem::copy(
        target,
        dest,
        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
        ec
    );
    if (!ec) {
        return LinkMethod::Copy;
    }

    throw PkgError(
        ErrorCode::LocalLinkFailed,
        "failed to link or copy local directory from '" + target.string() +
            "' to '" + dest.string() + "': " + ec.message()
    );
}

void Fetcher::remove(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec)) {
        std::filesystem::remove(path, ec);
    } else if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove_all(path, ec);
    }
}

} // namespace keety::pkg
