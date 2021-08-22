#pragma once

#include "VulkanResource.hpp"

#include <vector>

namespace crisp
{
    class VulkanDevice;

    class VulkanSwapChain : public VulkanResource<VkSwapchainKHR, vkDestroySwapchainKHR>
    {
    public:
        VulkanSwapChain(VulkanDevice* device, bool tripleBuffering);
        ~VulkanSwapChain();

        VulkanSwapChain(const VulkanSwapChain& other) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;
        VulkanSwapChain& operator=(VulkanSwapChain&& other) = delete;

        VkFormat getImageFormat() const;
        VkExtent2D getExtent() const;
        VkViewport createViewport(float minDepth = 0.0f, float maxDepth = 1.0f) const;
        VkRect2D createScissor() const;

        VkImageView getImageView(size_t index) const;
        uint32_t getSwapChainImageCount() const;

        void recreate();

    private:
        void createSwapChain();
        void createSwapChainImageViews();

        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR presentMode) const;
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

        bool       m_tripleBuffering;
        VkFormat   m_imageFormat;
        VkExtent2D m_extent;
        std::vector<VkImage>     m_images;
        std::vector<VkImageView> m_imageViews;
    };
}