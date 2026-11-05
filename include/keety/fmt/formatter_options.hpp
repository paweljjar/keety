#pragma once

#include <cstdint>

namespace keety::fmt {

/// Formatting configuration options
struct FormatterOptions {
    uint32_t indentWidth{4};            // Standard indentation (spaces)
    uint32_t maxLineWidth{100};         // Soft column wrap limit
    bool sortImports{true};             // Deterministic 3-tiered import sorting
    bool insertTrailingCommas{true};    // Trailing commas in multi-line literals
};

} // namespace keety::fmt
