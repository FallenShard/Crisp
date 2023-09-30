#pragma once

#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp {
VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance);

template <typename T>
concept VulkanWrapperType = requires(T t) { t.getHandle(); };

template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template <typename T>
concept VulkanHandle = IsAnyOf<
    T,
    VkInstance,
    VkPhysicalDevice,
    VkDevice,
    VkQueue,
    VkSemaphore,
    VkCommandBuffer,
    VkFence,
    VkDeviceMemory,
    VkBuffer,
    VkImage,
    VkEvent,
    VkQueryPool,
    VkBufferView,
    VkImageView,
    VkShaderModule,
    VkPipelineCache,
    VkPipelineLayout,
    VkRenderPass,
    VkPipeline,
    VkDescriptorSetLayout,
    VkSampler,
    VkDescriptorPool,
    VkDescriptorSet,
    VkFramebuffer,
    VkCommandPool,
    VkSamplerYcbcrConversion,
    VkDescriptorUpdateTemplate,
    VkPrivateDataSlot,
    VkSurfaceKHR,
    VkSwapchainKHR,
    VkDebugReportCallbackEXT,
    VkDisplayKHR,
    VkDisplayModeKHR,
    VkValidationCacheEXT,
    VkAccelerationStructureKHR>;

class VulkanDebugMarker {
public:
    explicit VulkanDebugMarker(VkDevice device);

    template <VulkanHandle T>
    void setObjectName(const T vulkanHandle, const char* name) const {
        setObjectName(reinterpret_cast<uint64_t>(vulkanHandle), name, getDebugReportObjectType<T>()); // NOLINT
    }

    template <VulkanHandle T>
    void setObjectName(const T vulkanHandle, const std::string& name) const {
        setObjectName(reinterpret_cast<uint64_t>(vulkanHandle), name.c_str(), getDebugReportObjectType<T>()); // NOLINT
    }

    template <VulkanWrapperType T>
    void setObjectName(const T& wrapperType, const char* name) const {
        setObjectName(
            reinterpret_cast<uint64_t>(wrapperType.getHandle()), // NOLINT
            name,
            getDebugReportObjectType<decltype(wrapperType.getHandle())>());
    }

private:
    VkDevice m_device;

    void setObjectName(uint64_t vulkanHandle, const char* name, VkObjectType objectType) const;

    template <typename VkHandle>
    static VkObjectType getDebugReportObjectType() {
        if constexpr (std::same_as<VkHandle, VkInstance>) {
            return VK_OBJECT_TYPE_INSTANCE;
        } else if constexpr (std::same_as<VkHandle, VkPhysicalDevice>) {
            return VK_OBJECT_TYPE_PHYSICAL_DEVICE;
        } else if constexpr (std::same_as<VkHandle, VkDevice>) {
            return VK_OBJECT_TYPE_DEVICE;
        } else if constexpr (std::same_as<VkHandle, VkQueue>) {
            return VK_OBJECT_TYPE_QUEUE;
        } else if constexpr (std::same_as<VkHandle, VkSemaphore>) {
            return VK_OBJECT_TYPE_SEMAPHORE;
        } else if constexpr (std::same_as<VkHandle, VkCommandBuffer>) {
            return VK_OBJECT_TYPE_COMMAND_BUFFER;
        } else if constexpr (std::same_as<VkHandle, VkFence>) {
            return VK_OBJECT_TYPE_FENCE;
        } else if constexpr (std::same_as<VkHandle, VkDeviceMemory>) {
            return VK_OBJECT_TYPE_DEVICE_MEMORY;
        } else if constexpr (std::same_as<VkHandle, VkBuffer>) {
            return VK_OBJECT_TYPE_BUFFER;
        } else if constexpr (std::same_as<VkHandle, VkImage>) {
            return VK_OBJECT_TYPE_IMAGE;
        } else if constexpr (std::same_as<VkHandle, VkEvent>) {
            return VK_OBJECT_TYPE_EVENT;
        } else if constexpr (std::same_as<VkHandle, VkQueryPool>) {
            return VK_OBJECT_TYPE_QUERY_POOL;
        } else if constexpr (std::same_as<VkHandle, VkBufferView>) {
            return VK_OBJECT_TYPE_BUFFER_VIEW;
        } else if constexpr (std::same_as<VkHandle, VkImageView>) {
            return VK_OBJECT_TYPE_IMAGE_VIEW;
        } else if constexpr (std::same_as<VkHandle, VkShaderModule>) {
            return VK_OBJECT_TYPE_SHADER_MODULE;
        } else if constexpr (std::same_as<VkHandle, VkPipelineCache>) {
            return VK_OBJECT_TYPE_PIPELINE_CACHE;
        } else if constexpr (std::same_as<VkHandle, VkPipelineLayout>) {
            return VK_OBJECT_TYPE_PIPELINE_LAYOUT;
        } else if constexpr (std::same_as<VkHandle, VkRenderPass>) {
            return VK_OBJECT_TYPE_RENDER_PASS;
        } else if constexpr (std::same_as<VkHandle, VkPipeline>) {
            return VK_OBJECT_TYPE_PIPELINE;
        } else if constexpr (std::same_as<VkHandle, VkDescriptorSetLayout>) {
            return VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
        } else if constexpr (std::same_as<VkHandle, VkSampler>) {
            return VK_OBJECT_TYPE_SAMPLER;
        } else if constexpr (std::same_as<VkHandle, VkDescriptorPool>) {
            return VK_OBJECT_TYPE_DESCRIPTOR_POOL;
        } else if constexpr (std::same_as<VkHandle, VkDescriptorSet>) {
            return VK_OBJECT_TYPE_DESCRIPTOR_SET;
        } else if constexpr (std::same_as<VkHandle, VkFramebuffer>) {
            return VK_OBJECT_TYPE_FRAMEBUFFER;
        } else if constexpr (std::same_as<VkHandle, VkCommandPool>) {
            return VK_OBJECT_TYPE_COMMAND_POOL;
        } else if constexpr (std::same_as<VkHandle, VkSamplerYcbcrConversion>) {
            return VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION;
        } else if constexpr (std::same_as<VkHandle, VkDescriptorUpdateTemplate>) {
            return VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE;
        } else if constexpr (std::same_as<VkHandle, VkPrivateDataSlot>) {
            return VK_OBJECT_TYPE_PRIVATE_DATA_SLOT;
        } else if constexpr (std::same_as<VkHandle, VkSurfaceKHR>) {
            return VK_OBJECT_TYPE_SURFACE_KHR;
        } else if constexpr (std::same_as<VkHandle, VkSwapchainKHR>) {
            return VK_OBJECT_TYPE_SWAPCHAIN_KHR;
        } else if constexpr (std::same_as<VkHandle, VkDebugReportCallbackEXT>) {
            return VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
        } else if constexpr (std::same_as<VkHandle, VkDisplayKHR>) {
            return VK_OBJECT_TYPE_DISPLAY_KHR;
        } else if constexpr (std::same_as<VkHandle, VkDisplayModeKHR>) {
            return VK_OBJECT_TYPE_DISPLAY_MODE_KHR;
        } else if constexpr (std::same_as<VkHandle, VkValidationCacheEXT>) {
            return VK_OBJECT_TYPE_VALIDATION_CACHE_EXT;
        } else if constexpr (std::same_as<VkHandle, VkAccelerationStructureKHR>) {
            return VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
        } else {
            CRISP_FATAL("Received unknown type in debug marker: {}.", typeid(VkHandle).name());
        }
    }
};
} // namespace crisp