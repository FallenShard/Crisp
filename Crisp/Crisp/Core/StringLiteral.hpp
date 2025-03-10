#pragma once

#include <string_view>

namespace crisp {

class LiteralWrapper {
public:
    template <size_t N>
    consteval LiteralWrapper(const char (&literal)[N]) // NOLINT
        : literal(literal) {}

    const char* literal{nullptr};

    constexpr bool operator==(const std::string_view& other) const noexcept {
        return other == std::string_view(literal);
    }
};
} // namespace crisp