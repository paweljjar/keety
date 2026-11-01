#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace keet {

struct error;

inline constexpr std::string_view type_of(int64_t) noexcept { return "int"; }
inline constexpr std::string_view type_of(int) noexcept { return "int"; }
inline constexpr std::string_view type_of(double) noexcept { return "float"; }
inline constexpr std::string_view type_of(float) noexcept { return "float"; }
inline constexpr std::string_view type_of(bool) noexcept { return "bool"; }
inline constexpr std::string_view type_of(char) noexcept { return "char"; }
inline constexpr std::string_view type_of(const std::string&) noexcept { return "string"; }
inline constexpr std::string_view type_of(const char*) noexcept { return "string"; }
inline constexpr std::string_view type_of(std::monostate) noexcept { return "null"; }

template <typename T>
inline constexpr std::string_view type_of(const std::vector<T>&) noexcept { return "array"; }

template <typename K, typename V>
inline constexpr std::string_view type_of(const std::unordered_map<K, V>&) noexcept { return "map"; }

template <typename... Ts>
inline bool is_type(const std::variant<Ts...>& v, std::string_view expected) {
    if constexpr ((std::is_same_v<Ts, int64_t> || ...)) {
        if (expected == "int") return std::holds_alternative<int64_t>(v);
    }
    if constexpr ((std::is_same_v<Ts, std::string> || ...)) {
        if (expected == "string") return std::holds_alternative<std::string>(v);
    }
    if constexpr ((std::is_same_v<Ts, double> || ...)) {
        if (expected == "float") return std::holds_alternative<double>(v);
    }
    if constexpr ((std::is_same_v<Ts, bool> || ...)) {
        if (expected == "bool") return std::holds_alternative<bool>(v);
    }
    if constexpr ((std::is_same_v<Ts, char> || ...)) {
        if (expected == "char") return std::holds_alternative<char>(v);
    }
    if constexpr ((std::is_same_v<Ts, std::monostate> || ...)) {
        if (expected == "null") return std::holds_alternative<std::monostate>(v);
    }
    return std::visit([](const auto& arg) -> std::string_view { return type_of(arg); }, v) == expected;
}

template <typename Variant>
struct variant_type_info {
    const Variant& v;

    bool operator==(std::string_view expected) const {
        return is_type(v, expected);
    }
    bool operator==(const char* expected) const {
        return is_type(v, std::string_view(expected));
    }
    bool operator!=(std::string_view expected) const {
        return !is_type(v, expected);
    }
    bool operator!=(const char* expected) const {
        return !is_type(v, std::string_view(expected));
    }
    operator std::string_view() const {
        return std::visit([](const auto& arg) -> std::string_view { return type_of(arg); }, v);
    }
    operator std::string() const {
        return std::string(operator std::string_view());
    }
};

template <typename... Ts>
inline variant_type_info<std::variant<Ts...>> type_of(const std::variant<Ts...>& v) {
    return variant_type_info<std::variant<Ts...>>{v};
}

// Convert arbitrary values to string representation
template <typename T>
[[nodiscard]] inline std::string to_string(const T& val) {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return val;
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        return std::string(val);
    } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
        return std::string(1, val);
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return val ? "true" : "false";
    } else {
        return std::to_string(val);
    }
}

namespace detail {
template <typename T1, typename T2, typename = void>
struct branch_type_impl {
    using type = std::variant<std::decay_t<T1>, std::decay_t<T2>>;
};

template <typename T1, typename T2>
struct branch_type_impl<T1, T2, std::enable_if_t<std::is_same_v<std::decay_t<T1>, std::decay_t<T2>>>> {
    using type = std::decay_t<T1>;
};

template <typename T1, typename T2>
struct branch_type_impl<T1, T2, std::enable_if_t<
    !std::is_same_v<std::decay_t<T1>, std::decay_t<T2>> &&
    std::is_integral_v<std::decay_t<T1>> &&
    std::is_integral_v<std::decay_t<T2>> &&
    !std::is_same_v<std::decay_t<T1>, bool> &&
    !std::is_same_v<std::decay_t<T2>, bool>
>> {
    using type = std::conditional_t<
        (sizeof(std::decay_t<T1>) >= sizeof(int64_t) || sizeof(std::decay_t<T2>) >= sizeof(int64_t)),
        int64_t,
        std::common_type_t<std::decay_t<T1>, std::decay_t<T2>>
    >;
};

template <typename T1, typename T2>
struct branch_type_impl<T1, T2, std::enable_if_t<
    !std::is_same_v<std::decay_t<T1>, std::decay_t<T2>> &&
    ((std::is_same_v<std::decay_t<T1>, std::string> && std::is_convertible_v<std::decay_t<T2>, std::string>) ||
     (std::is_same_v<std::decay_t<T2>, std::string> && std::is_convertible_v<std::decay_t<T1>, std::string>))
>> {
    using type = std::string;
};
} // namespace detail

// Polymorphic conditional branch type
template <typename T1, typename T2>
using branch_type_t = typename detail::branch_type_impl<T1, T2>::type;

// Polymorphic conditional branch returning common type or union variant
template <typename Cond, typename T1, typename T2>
inline auto branch(Cond&& cond, T1&& t1, T2&& t2) {
    using R = branch_type_t<T1, T2>;
    if constexpr (!std::is_same_v<R, std::variant<std::decay_t<T1>, std::decay_t<T2>>>) {
        return cond ? static_cast<R>(std::forward<T1>(t1)) : static_cast<R>(std::forward<T2>(t2));
    } else {
        using V = std::variant<std::decay_t<T1>, std::decay_t<T2>>;
        return cond ? V(std::forward<T1>(t1)) : V(std::forward<T2>(t2));
    }
}

} // namespace keet

// Comparison operators for variant and monostate (representing KeetScript null)
template <typename... Ts>
inline bool operator==(const std::variant<Ts...>& v, std::monostate) noexcept {
    if constexpr ((std::is_same_v<Ts, std::monostate> || ...)) {
        return std::holds_alternative<std::monostate>(v);
    } else {
        return false;
    }
}

template <typename... Ts>
inline bool operator==(std::monostate, const std::variant<Ts...>& v) noexcept {
    return v == std::monostate{};
}

template <typename... Ts>
inline bool operator!=(const std::variant<Ts...>& v, std::monostate) noexcept {
    if constexpr ((std::is_same_v<Ts, std::monostate> || ...)) {
        return !std::holds_alternative<std::monostate>(v);
    } else {
        return true;
    }
}

template <typename... Ts>
inline bool operator!=(std::monostate, const std::variant<Ts...>& v) noexcept {
    return v != std::monostate{};
}

// Safe numeric accumulation for variants
template <typename T, typename... Ts>
inline std::enable_if_t<std::is_integral_v<T>, T&> operator+=(T& a, const std::variant<Ts...>& v) {
    std::visit([&](const auto& val) {
        if constexpr (std::is_integral_v<std::decay_t<decltype(val)>>) {
            a += val;
        }
    }, v);
    return a;
}
