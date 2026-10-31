#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace keet {

struct error;

inline std::string type_of(int64_t) { return "int"; }
inline std::string type_of(int) { return "int"; }
inline std::string type_of(double) { return "float"; }
inline std::string type_of(float) { return "float"; }
inline std::string type_of(bool) { return "bool"; }
inline std::string type_of(char) { return "char"; }
inline std::string type_of(const std::string&) { return "string"; }
inline std::string type_of(const char*) { return "string"; }
inline std::string type_of(std::monostate) { return "null"; }

template <typename T>
inline std::string type_of(const std::vector<T>&) { return "array"; }

template <typename K, typename V>
inline std::string type_of(const std::unordered_map<K, V>&) { return "map"; }

template <typename... Ts>
inline std::string type_of(const std::variant<Ts...>& v) {
    return std::visit([](const auto& arg) { return type_of(arg); }, v);
}

} // namespace keet

// Comparison operators for variant and monostate (representing KeetScript null)
template <typename... Ts>
inline bool operator==(const std::variant<Ts...>& v, std::monostate) {
    if constexpr ((std::is_same_v<Ts, std::monostate> || ...)) {
        return std::holds_alternative<std::monostate>(v);
    } else {
        return false;
    }
}

template <typename... Ts>
inline bool operator==(std::monostate, const std::variant<Ts...>& v) {
    return v == std::monostate{};
}

template <typename... Ts>
inline bool operator!=(const std::variant<Ts...>& v, std::monostate) {
    return !(v == std::monostate{});
}

template <typename... Ts>
inline bool operator!=(std::monostate, const std::variant<Ts...>& v) {
    return !(v == std::monostate{});
}
