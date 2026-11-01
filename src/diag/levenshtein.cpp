#include <keety/diag/levenshtein.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

namespace keety::diag {

size_t levenshtein_distance(std::string_view s1, std::string_view s2) {
    if (s1 == s2) return 0;
    if (s1.empty()) return s2.length();
    if (s2.empty()) return s1.length();

    // Ensure s1 is shorter to optimize memory to O(min(M, N))
    if (s1.length() > s2.length()) {
        std::swap(s1, s2);
    }

    const size_t m = s1.length();
    const size_t n = s2.length();

    std::vector<size_t> prev(m + 1);
    std::vector<size_t> curr(m + 1);

    for (size_t i = 0; i <= m; ++i) {
        prev[i] = i;
    }

    for (size_t j = 1; j <= n; ++j) {
        curr[0] = j;
        for (size_t i = 1; i <= m; ++i) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[i] = std::min({
                prev[i] + 1,       // deletion from s2
                curr[i - 1] + 1,   // insertion into s2
                prev[i - 1] + cost // substitution
            });
        }
        prev = curr;
    }

    return prev[m];
}

std::optional<std::string_view> find_closest_match(
    std::string_view target,
    const std::vector<std::string>& candidates,
    size_t max_distance) {
    if (target.empty() || candidates.empty()) {
        return std::nullopt;
    }

    size_t best_dist = max_distance + 1;
    const std::string* best_match = nullptr;

    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        size_t dist = levenshtein_distance(target, candidate);
        if (dist <= max_distance && dist < best_dist) {
            best_dist = dist;
            best_match = &candidate;
            if (dist == 0) break;
        }
    }

    if (best_match) {
        return std::string_view(*best_match);
    }
    return std::nullopt;
}

bool is_snake_case(std::string_view s) {
    if (s.empty()) return false;
    bool has_underscore = false;
    for (char c : s) {
        if (c == '_') {
            has_underscore = true;
        } else if (std::isupper(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return has_underscore;
}

std::string to_camel_case(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    bool capitalize_next = false;
    for (char c : s) {
        if (c == '_') {
            capitalize_next = true;
        } else {
            if (capitalize_next) {
                result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                capitalize_next = false;
            } else {
                result.push_back(c);
            }
        }
    }
    return result;
}

std::string to_snake_case(std::string_view s) {
    std::string result;
    result.reserve(s.size() + 4);
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (std::isupper(static_cast<unsigned char>(c))) {
            if (i > 0 && s[i - 1] != '_') {
                result.push_back('_');
            }
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

} // namespace keety::diag
