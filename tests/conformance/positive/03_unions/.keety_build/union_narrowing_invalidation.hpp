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

namespace keet_module_union_narrowing_invalidation {
    // --- Exported Function Prototypes ---
    int64_t mutateAndVerify();
    int64_t testGuardedReassignment();
    int64_t testNullableInvalidation();
    int64_t main();
} // namespace keet_module_union_narrowing_invalidation
