#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp::detail {
const char* toString(VkResult result) noexcept;

inline VkResult doVkAssert(const VkResult result, const LocationFormatString& formatString = {}) noexcept {
    if (result == VK_SUCCESS) {
        return VK_SUCCESS;
    }

    spdlog::critical(
        "VkResult: {}\nFile: {}\n({}:{}) -- Function: `{}` \nMessage: {}",
        toString(result),
        formatString.loc.file_name(),
        formatString.loc.line(),
        formatString.loc.column(),
        formatString.loc.function_name(),
        formatString.str);
    std::abort();
}
} // namespace crisp::detail

#ifdef _DEBUG
// clang-format off
// #define VK_CHECK(expr, ...) expr
#define VK_CHECK(expr, ...) crisp::detail::doVkAssert(expr __VA_OPT__(, fmt::format(__VA_ARGS__)))
// clang-format on
#else
#define VK_CHECK(expr, ...) expr
#endif
