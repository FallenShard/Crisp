#pragma once

#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPhysicalDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
enum class TripleBuffering : uint8_t { Enabled, Disabled };

class VulkanSwapChain : public VulkanResource<VkSwapchainKHR> {
public:
    VulkanSwapChain(
        const VulkanDevice& device,
        const VulkanPhysicalDevice& physicalDevice,
        VkSurfaceKHR surface,
        TripleBuffering tripleBuffering);
    ~VulkanSwapChain() override;

    VulkanSwapChain(const VulkanSwapChain& other) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;

    VulkanSwapChain(VulkanSwapChain&& other) noexcept = default;
    VulkanSwapChain& operator=(VulkanSwapChain&& other) noexcept = default;

    VkFormat getImageFormat() const;
    VkExtent2D getExtent() const;
    VkViewport getViewport(float minDepth = 0.0f, float maxDepth = 1.0f) const;
    VkRect2D getScissorRect() const;

    VkImageView getImageView(size_t index) const;
    uint32_t getImageCount() const;

    void recreate(const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice, VkSurfaceKHR surface);

private:
    void createSwapChain(const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice, VkSurfaceKHR surface);
    void createImageViews(const VulkanDevice& device);

    static Result<VkSurfaceFormatKHR> selectSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats, const VkSurfaceFormatKHR& surfaceFormat);
    static Result<VkPresentModeKHR> selectPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR presentMode);
    static VkExtent2D determineExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
    VkPresentModeKHR m_presentationMode;
};
} // namespace crisp