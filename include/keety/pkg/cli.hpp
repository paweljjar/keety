#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace keety::pkg {

/// Command-line interface dispatcher for `kattery`.
class Cli {
public:
    /// Executes a kattery command from standard argv arguments.
    [[nodiscard]] static int run(
        int argc,
        char* argv[],
        std::ostream& out = std::cout,
        std::ostream& err = std::cerr
    );

    /// Overload taking vector of arguments (excluding program binary name).
    [[nodiscard]] static int run(
        const std::vector<std::string>& args,
        std::ostream& out = std::cout,
        std::ostream& err = std::cerr
    );
};

} // namespace keety::pkg
