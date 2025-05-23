#pragma once

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif // _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <type_traits>

#include <volk.h>

#if !defined(VMA_STATIC_VULKAN_FUNCTIONS)
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#if !defined(VMA_DYNAMIC_VULKAN_FUNCTIONS)
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#endif
#define VMA_VULKAN_VERSION 1004000
#include <vk_mem_alloc.h>

namespace crisp {

VkResult loadVulkanLoaderFunctions();
VkResult loadVulkanInstanceFunctions(VkInstance instance);
VkResult loadVulkanDeviceFunctions(VkDevice device);

#define IF_CONSTEXPR_VK_DESTROY_FUNC(HandleType)                                                                       \
    if constexpr (std::is_same_v<T, Vk##HandleType>) {                                                                 \
        return vkDestroy##HandleType;                                                                                  \
    } else

template <typename T>
inline auto getDestroyFunc() {
    using VulkanDestroyFunc = void (*)(VkDevice, T, const VkAllocationCallbacks*);

    IF_CONSTEXPR_VK_DESTROY_FUNC(Buffer)
    IF_CONSTEXPR_VK_DESTROY_FUNC(CommandPool)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Framebuffer)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Image)
    IF_CONSTEXPR_VK_DESTROY_FUNC(ImageView)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Pipeline)
    IF_CONSTEXPR_VK_DESTROY_FUNC(PipelineLayout)
    IF_CONSTEXPR_VK_DESTROY_FUNC(RenderPass)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Sampler)
    IF_CONSTEXPR_VK_DESTROY_FUNC(SwapchainKHR)
    IF_CONSTEXPR_VK_DESTROY_FUNC(AccelerationStructureKHR)

    return static_cast<VulkanDestroyFunc>(nullptr);
}

} // namespace crisp
