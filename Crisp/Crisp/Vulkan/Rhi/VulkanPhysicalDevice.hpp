#pragma once

#include <optional>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/Rhi/VulkanInstance.hpp>

namespace crisp {

struct SurfaceSupport {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilySupport {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    std::optional<uint32_t> compute;
    std::optional<uint32_t> transfer;

    bool isComplete() const {
        return graphics && present && compute && transfer;
    }
};

class VulkanDeviceFeatureChain {
public:
    VulkanDeviceFeatureChain() = default;
    ~VulkanDeviceFeatureChain() = default;
    VulkanDeviceFeatureChain(const VulkanDeviceFeatureChain& other) = delete;
    VulkanDeviceFeatureChain& operator=(const VulkanDeviceFeatureChain& other) = delete;

    VulkanDeviceFeatureChain(VulkanDeviceFeatureChain&& other) noexcept = delete;
    VulkanDeviceFeatureChain& operator=(VulkanDeviceFeatureChain&& other) noexcept = delete;

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
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};

    void reset() {
        features.pNext = nullptr;
        linkedStructs.clear();
    }

    template <typename FeatureStruct>
    void link(FeatureStruct& featureStruct) {
        if (linkedStructs.contains(typeid(FeatureStruct))) {
            return;
        }
        featureStruct.pNext = features.pNext;
        features.pNext = &featureStruct;

        linkedStructs.emplace(typeid(FeatureStruct));
    }

    FlatHashSet<std::type_index> linkedStructs;
};

struct VulkanPhysicalDeviceCapabilities {
    FlatStringHashSet extensions;

    VulkanDeviceFeatureChain features;

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

struct DeviceRequirements {
    bool rayTracing{false};
    bool pageableMemory{false};
    bool meshShading{false};
};

class VulkanPhysicalDevice {
public:
    explicit VulkanPhysicalDevice(VkPhysicalDevice handle);
    ~VulkanPhysicalDevice() = default;

    VulkanPhysicalDevice(const VulkanPhysicalDevice& other) = delete;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice& other) = delete;

    VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept = default;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept = default;

    VkPhysicalDevice getHandle() const {
        return m_handle;
    }

    const VulkanPhysicalDeviceCapabilities& getCapabilities() const {
        return *m_capabilities;
    }

    const VkPhysicalDeviceFeatures2& getFeatures() const {
        return m_capabilities->features.features;
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

    bool supportsPresentation(uint32_t queueFamilyIndex, VkSurfaceKHR surface) const;
    QueueFamilySupport queryQueueFamilySupport(VkSurfaceKHR surface) const;
    SurfaceSupport querySurfaceSupport(VkSurfaceKHR surface) const;
    std::vector<VkQueueFamilyProperties> queryQueueFamilyProperties() const;

    Result<uint32_t> findMemoryType(uint32_t memoryTypeMask, VkMemoryPropertyFlags properties) const;
    Result<uint32_t> findMemoryType(VkMemoryPropertyFlags properties) const;
    Result<uint32_t> findDeviceImageMemoryType(VkDevice device) const;
    Result<uint32_t> findDeviceBufferMemoryType(VkDevice device) const;
    Result<uint32_t> findStagingBufferMemoryType(VkDevice device) const;

    Result<VkFormat> findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    Result<VkFormat> findSupportedDepthFormat() const;

    VkFormatProperties getFormatProperties(VkFormat format) const;

    const FlatStringHashSet& getAvailableExtensions() const;

private:
    bool supportsDeviceExtensions(const std::vector<std::string>& deviceExtensions) const;

    VkPhysicalDevice m_handle{VK_NULL_HANDLE}; // Implicitly cleaned up with VkInstance.

    std::unique_ptr<VulkanPhysicalDeviceCapabilities> m_capabilities;
};

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const VulkanInstance& instance);
std::vector<VkExtensionProperties> querySupportedExtensions(VkPhysicalDevice physicalDevice);

struct VulkanDeviceFeatureRequest {
    std::string extensionName;
    bool isOptional{false};
    std::function<bool(const VulkanPhysicalDevice&)> isSupportedFunc = [](const VulkanPhysicalDevice&) { return true; };
    std::function<void(VulkanDeviceFeatureChain&)> addFeatureFunc = [](VulkanDeviceFeatureChain&) {};

    std::vector<VulkanDeviceFeatureRequest> dependencies;

    void appendTo(FlatStringHashSet& extensionsToEnable, VulkanDeviceFeatureChain& featureChainToEnable) const;
};

std::vector<VulkanDeviceFeatureRequest> createDefaultFeatureRequests();
void addPageableMemoryFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);
void addRayTracingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);
void addMeshShadingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);

bool isPhysicalDeviceSuitable(
    const VulkanPhysicalDevice& physicalDevice,
    const VulkanInstance& instance,
    std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& supportedFeatures,
    FlatStringHashSet& supportedExtensions);
Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance,
    std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& supportedFeatures,
    FlatStringHashSet& supportedExtensions);

} // namespace crisp
