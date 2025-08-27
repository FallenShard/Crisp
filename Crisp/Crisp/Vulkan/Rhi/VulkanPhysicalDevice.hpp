#pragma once

#include <optional>
#include <string>
#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>

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

struct DeviceRequirements {
    bool rayTracing{false};
    bool pageableMemory{false};
    bool meshShading{false};
};

class VulkanPhysicalDevice {
public:
    explicit VulkanPhysicalDevice(VkPhysicalDevice handle, const DeviceRequirements& requirements = {});
    ~VulkanPhysicalDevice() = default;

    VulkanPhysicalDevice(const VulkanPhysicalDevice& other) = delete;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice& other) = delete;

    VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept = default;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept = default;

    VkPhysicalDevice getHandle() const {
        return m_handle;
    }

    const VkPhysicalDeviceFeatures& getFeatures() const {
        return m_capabilities->features.features;
    }

    const VkPhysicalDeviceFeatures2& getFeatures2() const {
        return m_capabilities->features;
    }

    const VkPhysicalDeviceProperties& getProperties() const {
        return m_capabilities->properties.properties;
    }

    const VkPhysicalDeviceLimits& getLimits() const {
        return m_capabilities->properties.properties.limits;
    }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& getRayTracingPipelineProperties() const {
        return m_capabilities->rayTracingProperties;
    }

    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const {
        return m_capabilities->memoryProperties.memoryProperties;
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
    bool supportsDeviceExtensions(const std::vector<std::string>& deviceExtensions) const;

    VkPhysicalDevice m_handle{VK_NULL_HANDLE}; // Implicitly cleaned up with VkInstance

    struct Capabilities {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features features14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT};
        VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
        VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};

        VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        VkPhysicalDeviceVulkan11Properties properties11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};
        VkPhysicalDeviceVulkan12Properties properties12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
        VkPhysicalDeviceVulkan13Properties properties13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
        VkPhysicalDeviceVulkan14Properties properties14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};

        VkPhysicalDeviceMemoryProperties2 memoryProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    };

    std::unique_ptr<Capabilities> m_capabilities;
    std::vector<std::string> m_deviceExtensions;
};

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const VulkanInstance& instance);
std::vector<VkExtensionProperties> querySupportedExtensions(VkPhysicalDevice physicalDevice);

std::vector<std::string> createDefaultDeviceExtensions();
void addPageableMemoryDeviceExtensions(std::vector<std::string>& deviceExtensions);
void addRayTracingDeviceExtensions(std::vector<std::string>& deviceExtensions);
void addMeshShadingDeviceExtensions(std::vector<std::string>& deviceExtensions);

Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance, std::vector<std::string>&& deviceExtensions);

} // namespace crisp
