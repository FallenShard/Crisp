#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;

    class VulkanSwapChain
    {
    public:
        VulkanSwapChain(VulkanDevice* device);
        ~VulkanSwapChain();

        VulkanSwapChain(const VulkanSwapChain& other) = delete;
        VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;
        VulkanSwapChain& operator=(VulkanSwapChain&& other) = delete;

        VkSwapchainKHR getHandle() const;
        VkFormat getImageFormat() const;
        VkExtent2D getExtent() const;
        VkImageView getImageView(size_t index) const;

        void recreate();

    private:
        void createSwapChain();
        void createSwapImageViews();

        VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
        VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
        VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

        VulkanDevice* m_device;
        VkSwapchainKHR m_swapChain;

        VkFormat m_imageFormat;
        VkExtent2D m_extent;
        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_imageViews;
    };
}