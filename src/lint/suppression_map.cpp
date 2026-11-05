#include <keety/lint/suppression_map.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace keety::lint {

namespace {

std::string trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

std::string toLower(std::string_view str) {
    std::string res;
    res.reserve(str.size());
    for (char c : str) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

std::vector<std::string> splitRuleList(std::string_view text) {
    std::vector<std::string> rules;
    size_t start = 0;
    while (start < text.size()) {
        size_t comma = text.find(',', start);
        std::string_view part = (comma == std::string_view::npos)
            ? text.substr(start)
            : text.substr(start, comma - start);
        std::string rule = toLower(trim(part));
        if (!rule.empty()) {
            rules.push_back(std::move(rule));
        }
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return rules;
}

} // namespace

void SuppressionMap::addLineSuppression(size_t line, std::string_view rule) {
    lineSuppressions_[line].insert(toLower(rule));
}

void SuppressionMap::addRangeSuppression(size_t startLine, size_t endLine, std::string_view rule) {
    rangeSuppressions_.push_back(RangeSuppression{startLine, endLine, toLower(rule)});
}

bool SuppressionMap::isSuppressed(size_t line, std::string_view ruleName, std::string_view ruleCode) const {
    std::string nameLower = toLower(ruleName);
    std::string codeLower = toLower(ruleCode);

    // 1. Check line-specific suppressions (e.g. from disable-next-line)
    auto itLine = lineSuppressions_.find(line);
    if (itLine != lineSuppressions_.end()) {
        const auto& set = itLine->second;
        if (set.contains("*") || set.contains(nameLower) || set.contains(codeLower)) {
            return true;
        }
    }

    // 2. Check range suppressions (e.g. from disable ... enable blocks)
    for (const auto& r : rangeSuppressions_) {
        if (line >= r.startLine && line <= r.endLine) {
            if (r.rule == "*" || r.rule == nameLower || r.rule == codeLower) {
                return true;
            }
        }
    }

    return false;
}

void SuppressionMap::parseSuppressionDirective(
    std::string_view commentText,
    size_t commentLine,
    SuppressionMap& map,
    std::unordered_map<std::string, size_t>& activeBlocks
) {
    // Strip leading // or /* and trailing */
    std::string_view text = commentText;
    if (text.starts_with("//")) {
        text.remove_prefix(2);
    } else if (text.starts_with("/*")) {
        text.remove_prefix(2);
        if (text.ends_with("*/")) {
            text.remove_suffix(2);
        }
    }

    std::string trimmed = trim(text);
    std::string lower = toLower(trimmed);

    constexpr std::string_view PREFIX = "keetlint:";
    auto prefixPos = lower.find(PREFIX);
    if (prefixPos == std::string::npos) {
        return;
    }

    std::string_view directive = std::string_view(trimmed).substr(prefixPos + PREFIX.size());
    directive = std::string_view(trimmed).substr(prefixPos + PREFIX.size());
    while (!directive.empty() && std::isspace(static_cast<unsigned char>(directive.front()))) {
        directive.remove_prefix(1);
    }

    std::string directiveLower = toLower(directive);

    // 1. keetlint:disable-next-line [<rules...>]
    constexpr std::string_view DNL = "disable-next-line";
    if (directiveLower.starts_with(DNL)) {
        std::string_view rest = directive.substr(DNL.size());
        auto rules = splitRuleList(rest);
        if (rules.empty()) {
            map.addLineSuppression(commentLine + 1, "*");
        } else {
            for (const auto& r : rules) {
                map.addLineSuppression(commentLine + 1, r);
            }
        }
        return;
    }

    // 2. keetlint:enable [<rules...>]
    constexpr std::string_view EN = "enable";
    if (directiveLower.starts_with(EN)) {
        std::string_view rest = directive.substr(EN.size());
        auto rules = splitRuleList(rest);
        if (rules.empty()) {
            // Close all active blocks
            for (auto& [rule, startLine] : activeBlocks) {
                map.addRangeSuppression(startLine, commentLine, rule);
            }
            activeBlocks.clear();
        } else {
            for (const auto& r : rules) {
                auto it = activeBlocks.find(r);
                if (it != activeBlocks.end()) {
                    map.addRangeSuppression(it->second, commentLine, it->first);
                    activeBlocks.erase(it);
                }
            }
        }
        return;
    }

    // 3. keetlint:disable [<rules...>]
    constexpr std::string_view DIS = "disable";
    if (directiveLower.starts_with(DIS)) {
        std::string_view rest = directive.substr(DIS.size());
        auto rules = splitRuleList(rest);
        if (rules.empty()) {
            activeBlocks["*"] = commentLine;
        } else {
            for (const auto& r : rules) {
                activeBlocks[r] = commentLine;
            }
        }
        return;
    }
}

SuppressionMap SuppressionMap::fromComments(const std::vector<lexer::Comment>& comments) {
    SuppressionMap map;
    std::unordered_map<std::string, size_t> activeBlocks;

    for (const auto& comment : comments) {
        parseSuppressionDirective(comment.text, comment.span.start.line, map, activeBlocks);
    }

    // Any remaining open disable blocks stay active through end of file
    for (const auto& [rule, startLine] : activeBlocks) {
        map.addRangeSuppression(startLine, SIZE_MAX, rule);
    }

    return map;
}

SuppressionMap SuppressionMap::fromSource(std::string_view source) {
    lexer::Lexer lexer(1, source);
    lexer.setCaptureComments(true);
    std::vector<lexer::Comment> comments;

    while (true) {
        auto tok = lexer.nextToken();
        if (tok.is(lexer::TokenKind::Eof)) break;
    }

    comments = lexer.getCapturedComments();
    return fromComments(comments);
}

} // namespace keety::lint
