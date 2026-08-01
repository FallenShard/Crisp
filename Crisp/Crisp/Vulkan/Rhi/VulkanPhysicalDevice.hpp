#pragma once

#include <algorithm>
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

    bool isComplete(const bool requirePresentation) const {
        return graphics && compute && transfer && (!requirePresentation || present);
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
    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT};
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR};

    // Clears both the chain and every feature value. Device selection reuses one chain across candidates, so
    // without zeroing the values a rejected device's requests would leak into the next candidate's create info.
    void reset() {
        const auto clear = [](auto& featureStruct) {
            const auto sType = featureStruct.sType;
            featureStruct = {};
            featureStruct.sType = sType;
        };
        clear(features);
        clear(features11);
        clear(features12);
        clear(features13);
        clear(features14);
        clear(rayTracingFeatures);
        clear(accelerationStructureFeatures);
        clear(rayQueryFeatures);
        clear(pageableDeviceLocalMemoryFeatures);
        clear(meshShaderFeatures);
        clear(fragmentDensityMapFeatures);
        clear(fragmentShadingRateFeatures);
        linkedStructs.clear();
    }

    template <typename FeatureStruct>
    FeatureStruct& link(FeatureStruct& featureStruct) {
        if (linkedStructs.contains(typeid(FeatureStruct))) {
            return featureStruct;
        }
        featureStruct.pNext = features.pNext;
        features.pNext = &featureStruct;

        linkedStructs.emplace(typeid(FeatureStruct));
        return featureStruct;
    }

    FlatHashSet<std::type_index> linkedStructs;
};

struct VulkanPhysicalDeviceProperties {
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

struct VulkanDeviceFeatures {
    bool rayTracing{false};
    bool rayQuery{false};
    bool pageableMemory{false};
    bool meshShading{false};
};

namespace detail {
// The sType is not derivable from the type name by token pasting (VkPhysicalDeviceVulkan11Features carries
// VULKAN_1_1_FEATURES), so the pairing is spelled out once here - the only place a struct new to
// VulkanDeviceFeatureChain has to be registered. Kept in the header so queryFeatures resolves it at compile time
// and an unregistered struct fails the build rather than the run.
template <typename FeatureStruct>
consteval VkStructureType getFeatureStructureType() {
    if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceFeatures2>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceVulkan11Features>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceVulkan12Features>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceVulkan13Features>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceVulkan14Features>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceRayTracingPipelineFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceAccelerationStructureFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceRayQueryFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceMeshShaderFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceFragmentDensityMapFeaturesEXT>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
    } else if constexpr (std::is_same_v<FeatureStruct, VkPhysicalDeviceFragmentShadingRateFeaturesKHR>) {
        return VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
    } else {
        static_assert(!sizeof(FeatureStruct), "Failed to retrieve an sType for a Vulkan feature struct!");
    }
}
} // namespace detail

class VulkanPhysicalDevice {
public:
    VulkanPhysicalDevice(VkPhysicalDevice handle, uint32_t requestedApiVersion);
    ~VulkanPhysicalDevice() = default;

    VulkanPhysicalDevice(const VulkanPhysicalDevice& other) = delete;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice& other) = delete;

    VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept = default;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept = default;

    VkPhysicalDevice getHandle() const {
        return m_handle;
    }

    const VulkanPhysicalDeviceProperties& getCapabilities() const {
        return *m_capabilities;
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

    uint32_t getRequestedApiVersion() const {
        return m_requestedApiVersion;
    }

    uint32_t getSupportedApiVersion() const {
        return m_capabilities->properties.properties.apiVersion;
    }

    uint32_t getApiVersion() const {
        return std::min(m_requestedApiVersion, getSupportedApiVersion());
    }

    template <typename FeatureStruct>
    FeatureStruct queryFeatures() const {
        FeatureStruct featureStruct{detail::getFeatureStructureType<FeatureStruct>()};

        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &featureStruct;
        vkGetPhysicalDeviceFeatures2(m_handle, &features);
        return featureStruct;
    }

    VkPhysicalDeviceFeatures queryFeatures() const {
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        vkGetPhysicalDeviceFeatures2(m_handle, &features);
        return features.features;
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
    VkPhysicalDevice m_handle{VK_NULL_HANDLE}; // Implicitly cleaned up with VkInstance.

    uint32_t m_requestedApiVersion{VK_API_VERSION_1_0};
    FlatStringHashSet m_extensions;
    std::unique_ptr<VulkanPhysicalDeviceProperties> m_capabilities;
};

std::vector<VkPhysicalDevice> enumeratePhysicalDevices(const VulkanInstance& instance);
std::vector<VkExtensionProperties> querySupportedExtensions(VkPhysicalDevice physicalDevice);

struct VulkanDeviceFeatureRequest {
    std::string extensionName;
    std::string symbolicName;
    std::string getLoggingName() const;

    // Checked against VulkanPhysicalDevice::getApiVersion() before anything else, so isSupportedFunc never has to
    // guard against reading a feature struct the device is too old to populate.
    uint32_t minApiVersion{VK_API_VERSION_1_0};
    bool isRequired{true};
    std::function<bool(const VulkanPhysicalDevice&)> isSupportedFunc = [](const VulkanPhysicalDevice&) { return true; };
    std::function<void(VulkanDeviceFeatureChain&)> linkFunc = [](VulkanDeviceFeatureChain&) {};
    std::function<void(VulkanDeviceFeatures&)> setFunc = [](VulkanDeviceFeatures&) {};

    std::vector<VulkanDeviceFeatureRequest> prerequisites;

    bool isSupported(const VulkanPhysicalDevice& physicalDevice) const;
    void link(FlatStringHashSet& extensionsToEnable, VulkanDeviceFeatureChain& featureChainToEnable) const;
    void set(VulkanDeviceFeatures& features) const;
};

std::vector<VulkanDeviceFeatureRequest> createDefaultFeatureRequests();
void addPageableMemoryFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);
void addRayTracingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);
void addRayQueryFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);
void addMeshShadingFeatures(std::vector<VulkanDeviceFeatureRequest>& featureRequests);

bool isPhysicalDeviceSuitable(
    const VulkanPhysicalDevice& physicalDevice,
    const VulkanInstance& instance,
    std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& featureChainToEnable,
    FlatStringHashSet& extensionsToEnable,
    VulkanDeviceFeatures& supportedFeatures);
Result<VulkanPhysicalDevice> selectPhysicalDevice(
    const VulkanInstance& instance,
    std::span<const VulkanDeviceFeatureRequest> featureRequests,
    VulkanDeviceFeatureChain& featureChainToEnable,
    FlatStringHashSet& extensionsToEnable,
    VulkanDeviceFeatures& supportedFeatures);

} // namespace crisp
