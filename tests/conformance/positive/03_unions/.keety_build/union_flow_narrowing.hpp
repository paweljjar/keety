// Generated ahead-of-time by the KeetScript Compiler (keety). DO NOT EDIT.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <keet/runtime.hpp>

namespace keet_module_union_flow_narrowing {
    // --- Exported Function Prototypes ---
    std::string formatValue(const std::variant<int64_t, std::string, std::monostate>& v);
    int64_t getOrDefault(const std::variant<int64_t, std::monostate>& v, int64_t def);
    bool isPositiveInt(const std::variant<int64_t, std::string>& v);
    int64_t main();
} // namespace keet_module_union_flow_narrowing
