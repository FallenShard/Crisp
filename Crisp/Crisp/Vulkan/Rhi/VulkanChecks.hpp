#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp::detail {
const char* toString(VkResult result) noexcept;

[[noreturn]] inline void failVkCall(
    const char* expression, const VkResult result, const std::string& message, const std::source_location& loc) noexcept {
    failCheck(expression, fmt::format("\n  result = {}", toString(result)), message, loc);
}
} // namespace crisp::detail

// Always-on check — use for calls where failure is unrecoverable (allocation, submission, object creation).
// The message and the source location are only produced once the call has failed.
#define VK_FATAL(expr, ...)                                                                                            \
    do {                                                                                                               \
        const VkResult crispVkCallResult = (expr);                                                                     \
        if (crispVkCallResult != VK_SUCCESS) [[unlikely]] {                                                            \
            crisp::detail::failVkCall(                                                                                 \
                #expr,                                                                                                 \
                crispVkCallResult,                                                                                     \
                crisp::detail::makeCheckMessage(crisp::detail::CheckMessageTag {} __VA_OPT__(, ) __VA_ARGS__),         \
                std::source_location::current());                                                                      \
        }                                                                                                              \
    } while (false)

// Development-only check — use for hot-path calls where release overhead is undesirable. Unlike
// CRISP_DEV_CHECK, the expression is still evaluated in release: it is the Vulkan call itself.
#ifdef _DEBUG
#define VK_DEV_CHECK(expr, ...) VK_FATAL(expr __VA_OPT__(, ) __VA_ARGS__)
#else
// sizeof keeps the message arguments type-checked without evaluating them, so a release build cannot
// rot a development-only message into something that no longer compiles.
#define VK_DEV_CHECK(expr, ...)                                                                                        \
    do {                                                                                                               \
        (void)(expr);                                                                                                  \
        (void)sizeof(crisp::detail::discardCheckArgs(__VA_ARGS__));                                                    \
    } while (false)
#endif
