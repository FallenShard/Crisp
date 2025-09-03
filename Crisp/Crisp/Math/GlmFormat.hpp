#pragma once

#include <Crisp/Core/Format.hpp>
#include <Crisp/Math/Headers.hpp>

template <>
struct fmt::formatter<glm::vec2> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const glm::vec2& vec, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "[{:.4f}, {:.4f}]", vec.x, vec.y);
    }
};

template <>
struct fmt::formatter<glm::vec3> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const glm::vec3& vec, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "[{:.4f}, {:.4f}, {:.4f}]", vec.x, vec.y, vec.z);
    }
};

template <>
struct fmt::formatter<glm::vec4> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const glm::vec4& vec, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "[{:.4f}, {:.4f}, {:.4f}, {:.4f}]", vec.x, vec.y, vec.z, vec.w);
    }
};
