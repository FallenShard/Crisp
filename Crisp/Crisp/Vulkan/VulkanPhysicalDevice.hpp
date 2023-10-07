#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>

#include <Crisp/Core/Result.hpp>

#include <optional>
#include <string>
#include <vector>

namespace crisp {
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() &&
               transferFamily.has_value();
    }
};

struct SurfaceSupport {
    VkSurfaceKHR surface;
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanPhysicalDevice {
public:
    explicit VulkanPhysicalDevice(VkPhysicalDevice handle);
    ~VulkanPhysicalDevice() = default;

    VulkanPhysicalDevice(const VulkanPhysicalDevice& other) = delete;
    VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice& other) = delete;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept;

    inline VkPhysicalDevice getHandle() const {
        return m_handle;
    }

    inline const VkPhysicalDeviceFeatures& getFeatures() const {
        return m_features.features;
    }

    inline const VkPhysicalDeviceFeatures2& getFeatures2() const {
        return m_features;
    }

    inline const VkPhysicalDeviceProperties& getProperties() const {
        return m_properties.properties;
    }

    inline const VkPhysicalDeviceLimits& getLimits() const {
        return m_properties.properties.limits;
    }

    inline const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& getRayTracingPipelineProperties() const {
        return m_rayTracingPipelineProperties;
    }

    inline const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const {
        return m_memoryProperties.memoryProperties;
    }

    bool isSuitable(VkSurfaceKHR surface, const std::vector<std::string>& deviceExtensions) const;

    bool supportsPresentation(uint32_t queueFamilyIndex, VkSurfaceKHR surface) const;
    QueueFamilyIndices queryQueueFamilyIndices(VkSurfaceKHR surface) const;
    SurfaceSupport querySurfaceSupport(VkSurfaceKHR surface) const;
    std::vector<VkQueueFamilyProperties> queryQueueFamilyProperties() const;

    void setDeviceExtensions(std::vector<std::string>&& deviceExtensions);

    Result<uint32_t> findMemoryType(uint32_t memoryTypeMask, VkMemoryPropertyFlags properties) const;
    Result<uint32_t> findMemoryType(VkMemoryPropertyFlags properties) const;
    Result<uint32_t> findDeviceImageMemoryType(VkDevice device) const;
    Result<uint32_t> findDeviceBufferMemoryType(VkDevice device) const;
    Result<uint32_t> findStagingBufferMemoryType(VkDevice device) const;

    Result<VkFormat> findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    Result<VkFormat> findSupportedDepthFormat() const;

    VkFormatProperties getFormatProperties(VkFormat format) const;

    const std::vector<std::string>& getDeviceExtensions() const;

private:
    void initFeaturesAndProperties();

    bool supportsDeviceExtensions(const std::vector<std::string>& deviceExtensions) const;

    VkPhysicalDevice m_handle; // Implicitly cleaned up with VkInstance

    VkPhysicalDeviceFeatures2 m_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features m_features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features m_features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features m_features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rayTracingFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelerationStructureFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};

    VkPhysicalDeviceProperties2 m_properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    VkPhysicalDeviceVulkan11Properties m_properties11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
    VkPhysicalDeviceVulkan12Properties m_properties12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
    VkPhysicalDeviceVulkan13Properties m_properties13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceMemoryProperties2 m_memoryProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};

    std::vector<std::string> m_deviceExtensions;
};

std::vector<std::string> createDefaultDeviceExtensions();
void addRayTracingDeviceExtensions(std::vector<std::string>& deviceExtensions);

} // namespace crisp
