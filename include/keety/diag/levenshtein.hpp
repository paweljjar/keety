#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace keety::diag {

/// Computes Wagner-Fischer Levenshtein distance between s1 and s2 using O(min(M, N)) memory.
size_t levenshtein_distance(std::string_view s1, std::string_view s2);

/// Finds the closest candidate string to target whose distance is <= max_distance.
/// Returns nullopt if no candidate is within the threshold.
std::optional<std::string_view> find_closest_match(
    std::string_view target,
    const std::vector<std::string>& candidates,
    size_t max_distance = 3);

/// Checks if string is formatted in snake_case (contains '_' with alphanumeric characters).
bool is_snake_case(std::string_view s);

/// Converts snake_case string (e.g., "read_text") to camelCase ("readText").
std::string to_camel_case(std::string_view s);

/// Converts camelCase or PascalCase string to snake_case.
std::string to_snake_case(std::string_view s);

} // namespace keety::diag
