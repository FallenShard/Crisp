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

// Always-on check — use for calls where failure is unrecoverable (allocation, submission, object creation).
#define VK_FATAL(expr, ...) crisp::detail::doVkAssert(expr __VA_OPT__(, fmt::format(__VA_ARGS__)))

// Debug-only check — use for hot-path calls where release overhead is undesirable.
#ifdef _DEBUG
#define VK_CHECK(expr, ...) crisp::detail::doVkAssert(expr __VA_OPT__(, fmt::format(__VA_ARGS__)))
#else
#define VK_CHECK(expr, ...) expr
#endif
