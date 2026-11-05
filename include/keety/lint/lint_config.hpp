#pragma once

#include <keety/diag/diagnostic.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::lint {

/// Configuration options and rule severity overrides for the KeetScript Linter.
class LintConfig {
public:
    LintConfig() = default;

    /// Returns the global strictness flag (treats all warnings as errors if true).
    [[nodiscard]] bool isStrict() const noexcept { return strict_; }
    void setStrict(bool strict) noexcept { strict_ = strict; }

    /// Sets or overrides the severity of a rule by its ruleName or ruleCode.
    void setRuleSeverity(std::string_view ruleNameOrCode, std::optional<diag::DiagnosticSeverity> severity);

    /// Resolves the effective severity for a rule. Returns nullopt if the rule is disabled ("off").
    [[nodiscard]] std::optional<diag::DiagnosticSeverity> getRuleSeverity(
        std::string_view ruleName,
        std::string_view ruleCode,
        diag::DiagnosticSeverity defaultSeverity
    ) const;

    /// Checks if a file path is ignored by exclusion patterns.
    [[nodiscard]] bool isPathIgnored(const std::filesystem::path& path) const;

    /// Adds a path or glob pattern to the ignore list.
    void addIgnorePattern(std::string pattern) {
        ignorePatterns_.push_back(std::move(pattern));
    }

    /// Parses a JSON configuration string (e.g. from .keetlint.json or pedigree.json).
    static LintConfig parseJson(std::string_view jsonContent, std::string* outError = nullptr);

    /// Loads configuration from a file (.keetlint.json).
    static LintConfig loadFromFile(const std::filesystem::path& configPath, std::string* outError = nullptr);

    /// Automatically discovers and loads .keetlint.json starting from directory, walking upwards.
    static LintConfig discover(const std::filesystem::path& startPath);

private:
    bool strict_{false};
    std::map<std::string, std::optional<diag::DiagnosticSeverity>> ruleSeverities_;
    std::vector<std::string> ignorePatterns_;
};

} // namespace keety::lint
