#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

#include <Crisp/Common/Result.hpp>

#include <vector>

namespace crisp
{
class VulkanDevice;
class VulkanContext;

class VulkanSwapChain : public VulkanResource<VkSwapchainKHR, vkDestroySwapchainKHR>
{
public:
    VulkanSwapChain(const VulkanDevice& device, const VulkanContext& context, bool tripleBuffering);
    ~VulkanSwapChain();

    VulkanSwapChain(const VulkanSwapChain& other) = delete;
    VulkanSwapChain(VulkanSwapChain&& other) noexcept;
    VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;
    VulkanSwapChain& operator=(VulkanSwapChain&& other) noexcept;

    VkFormat getImageFormat() const;
    VkExtent2D getExtent() const;
    VkViewport getViewport(float minDepth = 0.0f, float maxDepth = 1.0f) const;
    VkRect2D getScissorRect() const;

    VkImageView getImageView(size_t index) const;
    uint32_t getSwapChainImageCount() const;

    void recreate(const VulkanDevice& device, const VulkanContext& context);

private:
    void createSwapChain(const VulkanDevice& device, const VulkanContext& context);
    void createSwapChainImageViews(VkDevice deviceHandle);

    Result<VkSurfaceFormatKHR> chooseSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats, const VkSurfaceFormatKHR& surfaceFormat) const;
    Result<VkPresentModeKHR> choosePresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR presentMode) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
    VkPresentModeKHR m_presentationMode;
};
} // namespace crisp