#pragma once

#include <Crisp/Common/Logger.hpp>
#include <Crisp/Math/Headers.hpp>

namespace fmt
{
template <>
struct formatter<glm::vec2>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec2& vec, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "[{}, {}]", vec.x, vec.y);
    }
};

template <>
struct formatter<glm::vec3>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec3& vec, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}]", vec.x, vec.y, vec.z);
    }
};

template <>
struct formatter<glm::vec4>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec4& vec, FormatContext& ctx)
    {
        return fmt::format_to(ctx.out(), "[{}, {}, {}, {}]", vec.x, vec.y, vec.z, vec.w);
    }
};
} // namespace fmt
