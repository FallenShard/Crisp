#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp::detail {
const char* toString(VkResult result) noexcept;

template <typename... Args>
VkResult doAssert(
    const VkResult result, const char* exprString, const LocationFormatString& formatString, Args&&... args) noexcept {
    if (result == VK_SUCCESS) {
        return VK_SUCCESS;
    }

    const auto argsFormat = fmt::format(formatString.str, std::forward<Args>(args)...);
    spdlog::critical(
        "File: {}\n({}:{}) -- Function: `{}`, Vulkan call '{}' returned '{}'\nMessage: {}",
        formatString.loc.file_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.loc.function_name(),
        exprString,
        toString(result),
        argsFormat);
    std::abort();
}

inline VkResult doAssert(const VkResult result, const LocationFormatString& formatString) noexcept {
    if (result == VK_SUCCESS) {
        return VK_SUCCESS;
    }

    spdlog::critical(
        "File: {}\n({}:{}) -- Function: `{}`, Vulkan call '{}' returned '{}'",
        formatString.loc.file_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.loc.function_name(),
        formatString.str,
        toString(result));
    std::abort();
}
} // namespace crisp::detail

#ifdef _DEBUG
// clang-format off
#define VK_CHECK(expr, ...) crisp::detail::doAssert(expr, #expr __VA_OPT__(,) __VA_ARGS__)
// clang-format on
#else
#define VK_CHECK(expr, ...) expr
#endif
