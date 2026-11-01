#pragma once

#include <keet/bool_byte.hpp>
#include <keet/error.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#include <stdio.h>
#define KEET_ISATTY(fd) _isatty(fd)
#define KEET_FILENO(stream) _fileno(stream)
#else
#include <stdio.h>
#include <unistd.h>
#define KEET_ISATTY(fd) isatty(fd)
#define KEET_FILENO(stream) fileno(stream)
#endif

namespace keet::io {

/// Terminal ANSI colors supported by the KeetScript standard io module.
enum class Color : int64_t {
    Default = 0,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    Gray
};

/// Returns the standard ANSI escape code associated with a Color.
[[nodiscard]] constexpr const char* color_code(Color c) noexcept {
    switch (c) {
    case Color::Default: return "\033[39m";
    case Color::Red:     return "\033[31m";
    case Color::Green:   return "\033[32m";
    case Color::Yellow:  return "\033[33m";
    case Color::Blue:    return "\033[34m";
    case Color::Magenta: return "\033[35m";
    case Color::Cyan:    return "\033[36m";
    case Color::White:   return "\033[37m";
    case Color::Gray:    return "\033[90m";
    }
    return "\033[39m";
}

namespace detail {

inline std::optional<bool>& color_override_ref() noexcept {
    static std::optional<bool> override_val{std::nullopt};
    return override_val;
}

[[nodiscard]] inline bool detect_terminal_stream(const std::ostream& out) noexcept {
    if (&out == &std::cout) {
        return KEET_ISATTY(KEET_FILENO(stdout)) != 0;
    }
    if (&out == &std::cerr) {
        return KEET_ISATTY(KEET_FILENO(stderr)) != 0;
    }
    return false;
}

template <typename T> struct is_vector : std::false_type {};
template <typename T, typename A> struct is_vector<std::vector<T, A>> : std::true_type {};
template <typename T> inline constexpr bool is_vector_v = is_vector<T>::value;

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};
template <typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_variant : std::false_type {};
template <typename... Ts> struct is_variant<std::variant<Ts...>> : std::true_type {};
template <typename T> inline constexpr bool is_variant_v = is_variant<T>::value;

template <typename T> struct is_map : std::false_type {};
template <typename K, typename V, typename H, typename E, typename A>
struct is_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template <typename K, typename V, typename C, typename A>
struct is_map<std::map<K, V, C, A>> : std::true_type {};
template <typename T> inline constexpr bool is_map_v = is_map<T>::value;

template <typename T, typename = void>
struct has_to_string : std::false_type {};

template <typename T>
struct has_to_string<T, std::void_t<decltype(std::declval<const T&>().toString())>> : std::true_type {};

template <typename T>
inline constexpr bool has_to_string_v = has_to_string<T>::value;

template <typename T, typename = void>
struct has_to_string_lower : std::false_type {};

template <typename T>
struct has_to_string_lower<T, std::void_t<decltype(std::declval<const T&>().to_string())>> : std::true_type {};

template <typename T>
inline constexpr bool has_to_string_lower_v = has_to_string_lower<T>::value;

template <typename T>
void print_single_arg(std::ostream& out, const T& arg) {
    using DecayT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayT, bool>) {
        out << (arg ? "true" : "false");
    } else if constexpr (std::is_same_v<DecayT, keet::BoolByte>) {
        out << (static_cast<bool>(arg) ? "true" : "false");
    } else if constexpr (std::is_same_v<DecayT, std::monostate> || std::is_same_v<DecayT, std::nullptr_t>) {
        out << "null";
    } else if constexpr (is_optional_v<DecayT>) {
        if (arg.has_value()) {
            print_single_arg(out, *arg);
        } else {
            out << "null";
        }
    } else if constexpr (is_variant_v<DecayT>) {
        std::visit([&out](const auto& val) {
            print_single_arg(out, val);
        }, arg);
    } else if constexpr (is_vector_v<DecayT>) {
        out << '[';
        for (size_t i = 0; i < arg.size(); ++i) {
            if (i > 0) out << ", ";
            print_single_arg(out, arg[i]);
        }
        out << ']';
    } else if constexpr (is_map_v<DecayT>) {
        out << '{';
        size_t idx = 0;
        for (const auto& [k, v] : arg) {
            if (idx > 0) out << ", ";
            print_single_arg(out, k);
            out << ": ";
            print_single_arg(out, v);
            ++idx;
        }
        out << '}';
    } else if constexpr (has_to_string_v<DecayT>) {
        out << arg.toString();
    } else if constexpr (has_to_string_lower_v<DecayT>) {
        out << arg.to_string();
    } else {
        out << arg;
    }
}

template <size_t Index = 0, typename Tuple>
void print_args_tuple(std::ostream& out, const Tuple& t) {
    if constexpr (Index < std::tuple_size_v<Tuple>) {
        if constexpr (Index > 0) {
            out << ' ';
        }
        print_single_arg(out, std::get<Index>(t));
        print_args_tuple<Index + 1>(out, t);
    }
}

template <typename Tuple, size_t... Is>
auto make_subtuple_impl(Tuple&& t, std::index_sequence<Is...>) {
    return std::forward_as_tuple(std::get<Is>(std::forward<Tuple>(t))...);
}

template <size_t N, typename Tuple>
auto take_first_n(Tuple&& t) {
    return make_subtuple_impl(std::forward<Tuple>(t), std::make_index_sequence<N>{});
}

template <typename... Ts>
struct last_type {
    using type = std::tuple_element_t<sizeof...(Ts) - 1, std::tuple<Ts...>>;
};

template <typename... Ts>
using last_type_t = typename last_type<Ts...>::type;

} // namespace detail

/// Manually enables or disables ANSI escape formatting.
inline void set_color_enabled(bool enabled) noexcept {
    detail::color_override_ref() = enabled;
}

/// Clears manual color override, reverting to NO_COLOR and TTY detection.
inline void reset_color_enabled() noexcept {
    detail::color_override_ref() = std::nullopt;
}

/// Checks whether standard output or the specified stream is connected to an interactive terminal.
[[nodiscard]] inline bool detect_terminal(const std::ostream& out = std::cout) noexcept {
    return detail::detect_terminal_stream(out);
}

/// Checks whether ANSI color escapes should be generated for the specified output stream.
/// Precedence:
/// 1. Explicit override via set_color_enabled(bool)
/// 2. NO_COLOR environment variable: if present (even if empty), disables automatic color
/// 3. Stream-specific TTY detection (isatty)
[[nodiscard]] inline bool is_color_enabled(const std::ostream& out = std::cout) noexcept {
    if (detail::color_override_ref().has_value()) {
        return *detail::color_override_ref();
    }
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color != nullptr) {
        return false;
    }
    return detail::detect_terminal_stream(out);
}

/// Wraps text with an ANSI prefix and suffixes with \033[0m if colors are enabled for the stream.
/// If text is empty or colors are disabled, returns unmodified text.
/// Restores enclosing style if text contains inner reset sequences (\033[0m).
[[nodiscard]] inline std::string style_wrap(std::string_view text, std::string_view prefix, const std::ostream& out = std::cout) {
    if (text.empty() || !is_color_enabled(out) || prefix.empty()) {
        return std::string(text);
    }
    std::string s(text);
    static constexpr std::string_view reset_seq = "\033[0m";
    size_t pos = 0;
    while ((pos = s.find(reset_seq, pos)) != std::string::npos) {
        if (pos + reset_seq.size() < s.size()) {
            pos += reset_seq.size();
            s.insert(pos, prefix);
            pos += prefix.size();
        } else {
            break;
        }
    }
    bool ends_with_reset = (s.size() >= reset_seq.size() &&
                            s.compare(s.size() - reset_seq.size(), reset_seq.size(), reset_seq) == 0);
    return std::string(prefix) + s + (ends_with_reset ? "" : "\033[0m");
}

/// Wraps text in ANSI escape sequence for the given color if enabled for the stream.
/// If text is empty or colors are disabled, returns unmodified text.
/// Restores enclosing color if text contains inner reset sequences (\033[0m).
[[nodiscard]] inline std::string colorize(std::string_view text, Color color, const std::ostream& out = std::cout) {
    if (color == Color::Default) {
        return std::string(text);
    }
    return style_wrap(text, color_code(color), out);
}

/// Wraps text in ANSI bold escape sequence if colors are enabled for the stream.
/// Restores enclosing bold attribute if text contains inner reset sequences (\033[0m).
[[nodiscard]] inline std::string bold(std::string_view s, const std::ostream& out = std::cout) {
    return style_wrap(s, "\033[1m", out);
}

[[nodiscard]] inline std::string red(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Red, out); }
[[nodiscard]] inline std::string green(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Green, out); }
[[nodiscard]] inline std::string yellow(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Yellow, out); }
[[nodiscard]] inline std::string blue(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Blue, out); }
[[nodiscard]] inline std::string magenta(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Magenta, out); }
[[nodiscard]] inline std::string cyan(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Cyan, out); }
[[nodiscard]] inline std::string white(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::White, out); }
[[nodiscard]] inline std::string gray(std::string_view s, const std::ostream& out = std::cout) { return colorize(s, Color::Gray, out); }

/// Flushes standard output or specified stream.
inline void flush(std::ostream& out = std::cout) {
    out.flush();
}

/// Outputs arguments to specified stream with space separation and optional trailing Color styling.
template <typename... Args>
void print_to(std::ostream& out, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        return;
    } else {
        using Last = detail::last_type_t<Args...>;
        if constexpr (std::is_same_v<std::decay_t<Last>, Color>) {
            auto t = std::forward_as_tuple(std::forward<Args>(args)...);
            Color color = std::get<sizeof...(Args) - 1>(t);
            auto printable = detail::take_first_n<sizeof...(Args) - 1>(t);
            if (is_color_enabled(out) && color != Color::Default) {
                out << color_code(color);
                detail::print_args_tuple(out, printable);
                out << "\033[0m";
            } else {
                detail::print_args_tuple(out, printable);
            }
        } else {
            auto t = std::forward_as_tuple(std::forward<Args>(args)...);
            detail::print_args_tuple(out, t);
        }
    }
}

/// Outputs arguments with trailing newline to specified stream with optional trailing Color styling.
template <typename... Args>
void println_to(std::ostream& out, Args&&... args) {
    print_to(out, std::forward<Args>(args)...);
    out << '\n';
}

/// Explicitly colored print function sending output to a specific stream.
template <typename... Args>
void print_colored_to(std::ostream& out, Color color, Args&&... args) {
    if (is_color_enabled(out) && color != Color::Default) {
        out << color_code(color);
        auto t = std::forward_as_tuple(std::forward<Args>(args)...);
        detail::print_args_tuple(out, t);
        out << "\033[0m";
    } else {
        auto t = std::forward_as_tuple(std::forward<Args>(args)...);
        detail::print_args_tuple(out, t);
    }
}

/// Explicitly colored println function sending output to a specific stream.
template <typename... Args>
void println_colored_to(std::ostream& out, Color color, Args&&... args) {
    print_colored_to(out, color, std::forward<Args>(args)...);
    out << '\n';
}

/// Variadic print function defaulting to std::cout or stream forwarding.
template <typename... Args>
void print(Args&&... args) {
    if constexpr (sizeof...(Args) > 0) {
        using First = std::tuple_element_t<0, std::tuple<Args...>>;
        if constexpr (std::is_base_of_v<std::ios_base, std::decay_t<First>>) {
            auto forward_to_stream = [](auto&& stream, auto&&... rest) {
                print_to(stream, std::forward<decltype(rest)>(rest)...);
            };
            forward_to_stream(std::forward<Args>(args)...);
        } else {
            print_to(std::cout, std::forward<Args>(args)...);
        }
    } else {
        print_to(std::cout);
    }
}

/// Variadic println function defaulting to std::cout or stream forwarding.
template <typename... Args>
void println(Args&&... args) {
    if constexpr (sizeof...(Args) > 0) {
        using First = std::tuple_element_t<0, std::tuple<Args...>>;
        if constexpr (std::is_base_of_v<std::ios_base, std::decay_t<First>>) {
            auto forward_to_stream = [](auto&& stream, auto&&... rest) {
                println_to(stream, std::forward<decltype(rest)>(rest)...);
            };
            forward_to_stream(std::forward<Args>(args)...);
        } else {
            println_to(std::cout, std::forward<Args>(args)...);
        }
    } else {
        println_to(std::cout);
    }
}

/// Standard error print function without trailing newline.
template <typename... Args>
void eprint(Args&&... args) {
    print_to(std::cerr, std::forward<Args>(args)...);
}

/// Standard error println function with trailing newline.
template <typename... Args>
void eprintln(Args&&... args) {
    println_to(std::cerr, std::forward<Args>(args)...);
}

/// Explicitly colored print function defaulting to std::cout.
template <typename... Args>
void print_colored(Color color, Args&&... args) {
    print_colored_to(std::cout, color, std::forward<Args>(args)...);
}

/// Explicitly colored println function defaulting to std::cout.
template <typename... Args>
void println_colored(Color color, Args&&... args) {
    println_colored_to(std::cout, color, std::forward<Args>(args)...);
}

/// Checks if EOF has been reached on the input stream.
[[nodiscard]] inline bool eof(std::istream& in = std::cin) noexcept {
    return in.eof();
}

/// Reads a line from stream, returning std::string.
/// Strips trailing '\r' (CRLF support).
/// Throws keet::error("IOError", ...) on hardware/stream failure (badbit).
/// Returns "" on EOF or empty line.
[[nodiscard]] inline std::string readln(std::istream& in = std::cin) {
    if (in.bad()) {
        throw error("IOError", "Input stream is in a bad state before read");
    }
    if (in.eof()) {
        return "";
    }
    std::string line;
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return line;
    }
    if (in.bad()) {
        throw error("IOError", "I/O error occurred while reading from input stream");
    }
    return "";
}

/// Reads a line from stream, returning std::variant<std::string, std::monostate>.
/// In KeetScript, this corresponds to `string | null`.
/// Returns std::string(line) on successful line read (including empty line "").
/// Returns std::monostate{} (null) on EOF.
/// Throws keet::error("IOError", ...) on hardware/stream failure (badbit).
[[nodiscard]] inline std::variant<std::string, std::monostate> readLine(std::istream& in = std::cin) {
    if (in.bad()) {
        throw error("IOError", "Input stream is in a bad state before read");
    }
    if (in.eof()) {
        return std::monostate{};
    }
    std::string line;
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return line;
    }
    if (in.bad()) {
        throw error("IOError", "I/O error occurred while reading from input stream");
    }
    return std::monostate{};
}

/// Reads a line from stream, returning std::optional<std::string>.
/// Returns std::string on success (including empty line "").
/// Returns std::nullopt on EOF.
/// Throws keet::error("IOError", ...) on hardware/stream failure (badbit).
[[nodiscard]] inline std::optional<std::string> readLineOpt(std::istream& in = std::cin) {
    auto res = readLine(in);
    if (std::holds_alternative<std::string>(res)) {
        return std::get<std::string>(res);
    }
    return std::nullopt;
}

/// Displays prompt, flushes stream immediately, and reads a line of input.
[[nodiscard]] inline std::string input(std::string_view prompt = "", std::istream& in = std::cin, std::ostream& out = std::cout) {
    if (!prompt.empty()) {
        out << prompt;
    }
    out.flush();
    return readln(in);
}

} // namespace keet::io
