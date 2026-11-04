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

namespace keet_module_union_ternary_narrowing {
    // --- Exported Function Prototypes ---
#line 1 "union_ternary_narrowing.keet"
    int64_t test_fallback(const std::variant<int64_t, std::monostate>& a);
#line 6 "union_ternary_narrowing.keet"
    int64_t test_narrowed(const std::variant<int64_t, std::monostate>& a);
#line 11 "union_ternary_narrowing.keet"
    int64_t main();
} // namespace keet_module_union_ternary_narrowing
