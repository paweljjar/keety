#pragma once

#include <keet/bool_byte.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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

[[nodiscard]] inline bool detect_terminal() noexcept {
    return KEET_ISATTY(KEET_FILENO(stdout)) != 0;
}

template <typename T>
void print_single_arg(std::ostream& out, const T& arg) {
    if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        out << (arg ? "true" : "false");
    } else if constexpr (std::is_same_v<std::decay_t<T>, keet::BoolByte>) {
        out << (static_cast<bool>(arg) ? "true" : "false");
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

/// Checks whether ANSI color escapes should be generated.
[[nodiscard]] inline bool is_color_enabled() noexcept {
    if (detail::color_override_ref().has_value()) {
        return *detail::color_override_ref();
    }
    const char* no_color = std::getenv("NO_COLOR");
    if (no_color != nullptr && no_color[0] != '\0') {
        return false;
    }
    return detail::detect_terminal();
}

/// Wraps text in ANSI escape sequence for the given color if enabled.
[[nodiscard]] inline std::string colorize(std::string_view text, Color color) {
    if (!is_color_enabled() || color == Color::Default) {
        return std::string(text);
    }
    return std::string(color_code(color)) + std::string(text) + "\033[0m";
}

/// Wraps text in ANSI bold escape sequence if colors are enabled.
[[nodiscard]] inline std::string bold(std::string_view s) {
    if (!is_color_enabled()) {
        return std::string(s);
    }
    return std::string("\033[1m") + std::string(s) + "\033[0m";
}

[[nodiscard]] inline std::string red(std::string_view s) { return colorize(s, Color::Red); }
[[nodiscard]] inline std::string green(std::string_view s) { return colorize(s, Color::Green); }
[[nodiscard]] inline std::string yellow(std::string_view s) { return colorize(s, Color::Yellow); }
[[nodiscard]] inline std::string blue(std::string_view s) { return colorize(s, Color::Blue); }
[[nodiscard]] inline std::string magenta(std::string_view s) { return colorize(s, Color::Magenta); }
[[nodiscard]] inline std::string cyan(std::string_view s) { return colorize(s, Color::Cyan); }
[[nodiscard]] inline std::string white(std::string_view s) { return colorize(s, Color::White); }
[[nodiscard]] inline std::string gray(std::string_view s) { return colorize(s, Color::Gray); }

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
            if (is_color_enabled() && color != Color::Default) {
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
    if (is_color_enabled() && color != Color::Default) {
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

/// Standard error print function.
template <typename... Args>
void eprint(Args&&... args) {
    print_to(std::cerr, std::forward<Args>(args)...);
}

/// Standard error println function.
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

/// Reads a line from stream, stripping any trailing '\r'.
[[nodiscard]] inline std::string readln(std::istream& in = std::cin) {
    std::string line;
    if (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
    }
    return line;
}

/// Alias for readln matching language specification naming.
[[nodiscard]] inline std::string readLine(std::istream& in = std::cin) {
    return readln(in);
}

/// Displays prompt, flushes stream, and reads a line of input.
[[nodiscard]] inline std::string input(std::string_view prompt = "", std::istream& in = std::cin, std::ostream& out = std::cout) {
    if (!prompt.empty()) {
        out << prompt;
    }
    out.flush();
    return readln(in);
}

} // namespace keet::io
