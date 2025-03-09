#pragma once

#include <functional>
#include <unordered_set>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
using SurfaceCreator = std::function<VkResult(VkInstance, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

namespace detail {
struct StringHash {
    using is_transparent = void;

    [[nodiscard]] size_t operator()(const char* txt) const {
        return std::hash<std::string_view>{}(txt);
    }

    [[nodiscard]] size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }

    [[nodiscard]] size_t operator()(const std::string& txt) const {
        return std::hash<std::string>{}(txt);
    }
};
} // namespace detail

class VulkanInstance {
public:
    VulkanInstance(
        const SurfaceCreator& surfaceCreator, std::vector<std::string> requiredExtensions, bool enableValidationLayers);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance& other) = delete;
    VulkanInstance(VulkanInstance&& other) = delete;
    VulkanInstance& operator=(const VulkanInstance& other) = delete;
    VulkanInstance& operator=(VulkanInstance&& other) = delete;

    VkInstance getHandle() const;
    VkSurfaceKHR getSurface() const;

    uint32_t getApiVersion() const;

    bool isExtensionEnabled(std::string_view extensionName) const;
    bool isLayerEnabled(std::string_view layerName) const;

private:
    VkInstance m_handle;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface;
    uint32_t m_apiVersion;

    std::unordered_set<std::string, detail::StringHash, std::equal_to<>> m_enabledExtensions;
    std::unordered_set<std::string, detail::StringHash, std::equal_to<>> m_enabledLayers;
};
} // namespace crisp
