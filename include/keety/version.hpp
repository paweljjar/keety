#pragma once

#include <string_view>

namespace keety {

[[nodiscard]] constexpr std::string_view getVersionString() noexcept {
    return "0.1.0";
}

} // namespace keety
