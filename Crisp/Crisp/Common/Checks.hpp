#pragma once

#include <Crisp/Common/Result.hpp>

namespace crisp::detail
{
template <typename... Args>
void doAssert(const bool expr, const char* exprString, LocationFormatString&& formatString, Args&&... args) noexcept
{
    if (expr)
        return;

    const auto argsFormat = fmt::format(fmt::runtime(formatString.str), std::forward<Args>(args)...);
    spdlog::critical(
        "File: {}\n({}:{}) -- Function: `{}`, Condition: {}\nMessage: {}",
        formatString.loc.file_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.loc.function_name(),
        exprString,
        argsFormat);
    std::abort();
}

inline void doAssert(const bool expr, LocationFormatString&& formatString) noexcept
{
    if (expr)
        return;

    spdlog::critical(
        "File: {}\n({}:{}) -- Function: `{}`, Condition: {}",
        formatString.loc.file_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.loc.function_name(),
        formatString.str); // We default-construct the formatString with the expression string in the macro.
    std::abort();
}
} // namespace crisp::detail

#ifdef _DEBUG
// clang-format off
#define CRISP_CHECK(expr, ...) crisp::detail::doAssert(expr, #expr __VA_OPT__(,) __VA_ARGS__)
#define CRISP_CHECK_GE_LT(expr, left, right, ...) crisp::detail::doAssert((expr >= (left)) && (expr < (right)), #expr __VA_OPT__(,) __VA_ARGS__)
#define CRISP_CHECK_EQ(expr, right, ...) crisp::detail::doAssert(expr == right, #expr __VA_OPT__(,) __VA_ARGS__)
// clang-format on
#else
#define CRISP_CHECK(expr, ...)
#define CRISP_CHECK_GE_LT(expr, left, right, ...)
#define CRISP_CHECK_EQ(expr, right, ...)
#endif