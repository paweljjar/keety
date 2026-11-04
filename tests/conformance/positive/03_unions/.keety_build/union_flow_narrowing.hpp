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
#line 1 "union_flow_narrowing.keet"
    std::string formatValue(const std::variant<int64_t, std::string, std::monostate>& v);
#line 16 "union_flow_narrowing.keet"
    int64_t getOrDefault(const std::variant<int64_t, std::monostate>& v, int64_t def);
#line 23 "union_flow_narrowing.keet"
    bool isPositiveInt(const std::variant<int64_t, std::string>& v);
#line 33 "union_flow_narrowing.keet"
    int64_t main();
} // namespace keet_module_union_flow_narrowing
