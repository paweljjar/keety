#include <keety/lint/lint_config.hpp>

#include <keet/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace keety::lint {

namespace {

std::string toLower(std::string_view str) {
    std::string res;
    res.reserve(str.size());
    for (char c : str) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

std::optional<diag::DiagnosticSeverity> parseSeverity(std::string_view val) {
    std::string s = toLower(val);
    if (s == "off" || s == "none" || s == "0" || s == "false") {
        return std::nullopt; // disabled
    }
    if (s == "error" || s == "err" || s == "2") {
        return diag::DiagnosticSeverity::Error;
    }
    if (s == "warn" || s == "warning" || s == "1") {
        return diag::DiagnosticSeverity::Warning;
    }
    if (s == "info" || s == "note" || s == "hint") {
        return diag::DiagnosticSeverity::Note;
    }
    return diag::DiagnosticSeverity::Warning;
}

} // namespace

void LintConfig::setRuleSeverity(std::string_view ruleNameOrCode, std::optional<diag::DiagnosticSeverity> severity) {
    ruleSeverities_[toLower(ruleNameOrCode)] = severity;
}

std::optional<diag::DiagnosticSeverity> LintConfig::getRuleSeverity(
    std::string_view ruleName,
    std::string_view ruleCode,
    diag::DiagnosticSeverity defaultSeverity
) const {
    // Check override by ruleName first
    auto itName = ruleSeverities_.find(toLower(ruleName));
    if (itName != ruleSeverities_.end()) {
        if (!itName->second.has_value()) {
            return std::nullopt; // disabled
        }
        return strict_ ? diag::DiagnosticSeverity::Error : itName->second.value();
    }

    // Check override by ruleCode second
    auto itCode = ruleSeverities_.find(toLower(ruleCode));
    if (itCode != ruleSeverities_.end()) {
        if (!itCode->second.has_value()) {
            return std::nullopt; // disabled
        }
        return strict_ ? diag::DiagnosticSeverity::Error : itCode->second.value();
    }

    // Fall back to default severity, elevated if strict
    if (strict_ && defaultSeverity == diag::DiagnosticSeverity::Warning) {
        return diag::DiagnosticSeverity::Error;
    }
    return defaultSeverity;
}

bool LintConfig::isPathIgnored(const std::filesystem::path& path) const {
    if (ignorePatterns_.empty()) {
        return false;
    }

    std::string pathStr = path.generic_string();
    for (const auto& pattern : ignorePatterns_) {
        if (pattern.empty()) continue;

        // Simple wildcard prefix or suffix matching
        if (pattern.ends_with("/**")) {
            std::string prefix = pattern.substr(0, pattern.size() - 3);
            if (pathStr.starts_with(prefix) || pathStr.find("/" + prefix) != std::string::npos) {
                return true;
            }
        } else if (pathStr.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

LintConfig LintConfig::parseJson(std::string_view jsonContent, std::string* outError) {
    LintConfig config;
    try {
        keet::json::detail::Parser parser(jsonContent);
        auto doc = parser.parse_document();
        if (!doc.is_object()) {
            if (outError) *outError = "Root of lint config must be a JSON object";
            return config;
        }

        const auto& root = std::get<keet::json::detail::JsonNode::Object>(doc.value);

        // Check for "strict"
        auto itStrict = root.find("strict");
        if (itStrict != root.end() && itStrict->second.is_bool()) {
            config.setStrict(std::get<bool>(itStrict->second.value));
        }

        // Check for "rules" object
        auto itRules = root.find("rules");
        if (itRules != root.end() && itRules->second.is_object()) {
            const auto& rulesObj = std::get<keet::json::detail::JsonNode::Object>(itRules->second.value);
            for (const auto& [ruleKey, sevVal] : rulesObj) {
                if (sevVal.is_string()) {
                    config.setRuleSeverity(ruleKey, parseSeverity(std::get<std::string>(sevVal.value)));
                } else if (sevVal.is_bool()) {
                    bool enabled = std::get<bool>(sevVal.value);
                    config.setRuleSeverity(ruleKey, enabled ? std::optional(diag::DiagnosticSeverity::Warning) : std::nullopt);
                }
            }
        }

        // Check for "ignore" array
        auto itIgnore = root.find("ignore");
        if (itIgnore != root.end() && itIgnore->second.is_array()) {
            const auto& arr = std::get<keet::json::detail::JsonNode::Array>(itIgnore->second.value);
            for (const auto& item : arr) {
                if (item.is_string()) {
                    config.addIgnorePattern(std::get<std::string>(item.value));
                }
            }
        }
    } catch (const std::exception& e) {
        if (outError) *outError = e.what();
    }
    return config;
}

LintConfig LintConfig::loadFromFile(const std::filesystem::path& configPath, std::string* outError) {
    std::ifstream ifs(configPath, std::ios::binary);
    if (!ifs) {
        if (outError) *outError = "Unable to open config file: " + configPath.string();
        return LintConfig{};
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parseJson(content, outError);
}

LintConfig LintConfig::discover(const std::filesystem::path& startPath) {
    std::filesystem::path current = std::filesystem::absolute(startPath);
    if (std::filesystem::is_regular_file(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        auto candidate = current / ".keetlint.json";
        if (std::filesystem::exists(candidate)) {
            return loadFromFile(candidate);
        }

        // Check pedigree.json for "lint" field
        auto pedigree = current / "pedigree.json";
        if (std::filesystem::exists(pedigree)) {
            std::ifstream ifs(pedigree, std::ios::binary);
            if (ifs) {
                std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                try {
                    keet::json::detail::Parser parser(content);
                    auto doc = parser.parse_document();
                    if (doc.is_object()) {
                        const auto& root = std::get<keet::json::detail::JsonNode::Object>(doc.value);
                        auto itLint = root.find("lint");
                        if (itLint != root.end()) {
                            std::string lintJson;
                            keet::json::detail::serialize_node(itLint->second, lintJson, 0, 0);
                            return parseJson(lintJson);
                        }
                    }
                } catch (...) {}
            }
        }

        auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return LintConfig{};
}

} // namespace keety::lint
