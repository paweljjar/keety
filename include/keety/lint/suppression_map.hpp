#pragma once

#include <keety/lexer/lexer.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace keety::lint {

/// Maps source lines and regions to suppressed lint rules parsed from source comments.
/// Supports:
/// - `// keetlint:disable-next-line <rule1>, <rule2>`
/// - `// keetlint:disable <rule1>, <rule2>`
/// - `// keetlint:enable <rule1>, <rule2>`
class SuppressionMap {
public:
    SuppressionMap() = default;

    /// Scans a vector of lexical comments and populates line and range suppressions.
    static SuppressionMap fromComments(const std::vector<lexer::Comment>& comments);

    /// Scans raw source code directly for suppression comments.
    static SuppressionMap fromSource(std::string_view source);

    /// Checks if a given rule (by name or code) is suppressed on a specific 1-indexed source line.
    [[nodiscard]] bool isSuppressed(size_t line, std::string_view ruleName, std::string_view ruleCode) const;

    /// Disables rules for a specific line (e.g. for disable-next-line).
    void addLineSuppression(size_t line, std::string_view rule);

    /// Adds a region suppression from startLine to endLine (or up to SIZE_MAX if open-ended).
    void addRangeSuppression(size_t startLine, size_t endLine, std::string_view rule);

private:
    // line -> set of suppressed rule names or codes ("*" means all rules)
    std::unordered_map<size_t, std::unordered_set<std::string>> lineSuppressions_;

    struct RangeSuppression {
        size_t startLine{0};
        size_t endLine{SIZE_MAX};
        std::string rule; // "*" means all rules
    };
    std::vector<RangeSuppression> rangeSuppressions_;

    static void parseSuppressionDirective(
        std::string_view commentText,
        size_t commentLine,
        SuppressionMap& map,
        std::unordered_map<std::string, size_t>& activeBlocks
    );
};

} // namespace keety::lint
