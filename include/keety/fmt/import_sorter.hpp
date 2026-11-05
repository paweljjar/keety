#pragma once

#include <keety/ast/decl.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace keety::fmt {

/// Tiers for deterministic import grouping
enum class ImportTier : uint8_t {
    StandardLibrary = 1, // Standard library: `io`, `fs`, `net.http`
    ThirdParty = 2,      // Third-party packages: `"github.com/..."`, external package
    LocalRelative = 3    // Local relative imports: `"./util"`, `"../core"`
};

class ImportSorter {
public:
    /// Classifies an import path into one of the 3 tiers
    [[nodiscard]] static ImportTier classify(std::string_view modulePath) noexcept;

    /// Sorts import declarations deterministically into 3 groups (tiers),
    /// where each group is sorted alphabetically.
    [[nodiscard]] static std::vector<std::vector<const ast::ImportDecl*>> groupAndSort(
        const std::vector<const ast::ImportDecl*>& imports
    );
};

} // namespace keety::fmt
