#pragma once

#include <exception>
#include <string>
#include <utility>

namespace keet {

/// Predeclared root error type for KeetScript runtime and generated C++ translation units.
/// Conforms directly to Section 4.1 of Keety Compiler Architecture Specification and
/// Section 'The predeclared error type' of The KeetScript Language Specification.
struct error : public std::exception {
    std::string name;
    std::string message;

    explicit error(std::string n = "Error", std::string m = "[Error]")
        : name(std::move(n)), message(std::move(m)) {}

    [[nodiscard]] std::string toString() const {
        return "[" + name + "]: " + message;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return message.c_str();
    }
};

} // namespace keet
