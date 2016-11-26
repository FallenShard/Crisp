#include "VulkanSwapChain.hpp"

#include <algorithm>
#include <array>

#include "VulkanDevice.hpp"
#include "VulkanContext.hpp"

namespace crisp
{
    VulkanSwapChain::VulkanSwapChain(const VulkanContext& context, const VulkanDevice& device)
        : m_swapChain(VK_NULL_HANDLE)
        , m_device(device.getHandle())
    {
        createSwapChain(context);
        createSwapImageViews(device);
    }

    VulkanSwapChain::~VulkanSwapChain()
    {
        for (auto imageView : m_imageViews)
            vkDestroyImageView(m_device, imageView, nullptr);
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    VkSwapchainKHR VulkanSwapChain::getHandle() const
    {
        return m_swapChain;
    }

    VkFormat VulkanSwapChain::getImageFormat() const
    {
        return m_imageFormat;
    }

    VkExtent2D VulkanSwapChain::getExtent() const
    {
        return m_extent;
    }

    VkImageView VulkanSwapChain::getImageView(size_t index) const
    {
        return m_imageViews.at(index);
    }

    void VulkanSwapChain::recreate(const VulkanContext& context, const VulkanDevice& device)
    {
        for (auto imageView : m_imageViews)
            vkDestroyImageView(m_device, imageView, nullptr);
        createSwapChain(context);
        createSwapImageViews(device);
    }

    void VulkanSwapChain::createSwapChain(const VulkanContext& context)
    {
        SwapChainSupportDetails swapChainSupport = context.querySwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode     = choosePresentMode(swapChainSupport.presentModes);
        VkExtent2D extent                = chooseExtent(swapChainSupport.capabilities);

        // minImageCount + 1 to support triple-buffering with MAILBOX present mode
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = context.getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Render directly to swap chain image

        QueueFamilyIndices indices = context.findQueueFamilies();
        std::array<uint32_t, 2> queueFamilyIndices =
        { 
            static_cast<uint32_t>(indices.graphicsFamily),
            static_cast<uint32_t>(indices.presentFamily)
        };
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = m_swapChain;

        vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain);

        if (createInfo.oldSwapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(m_device, createInfo.oldSwapchain, nullptr);

        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_images.data());

        m_imageFormat = surfaceFormat.format;
        m_extent = extent;
    }

    void VulkanSwapChain::createSwapImageViews(const VulkanDevice& device)
    {
        m_imageViews.resize(m_images.size(), { VK_NULL_HANDLE });

        for (uint32_t i = 0; i < m_images.size(); i++)
            m_imageViews[i] = device.createImageView(m_images[i], VK_IMAGE_VIEW_TYPE_2D, m_imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
    }

    VkSurfaceFormatKHR VulkanSwapChain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
            return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

        for (const auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        }

        return availableFormats[0];
    }

    VkPresentModeKHR VulkanSwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) // Triple-buffering
                return availablePresentMode;
        }

        return VK_PRESENT_MODE_FIFO_KHR; // V-Sync is FIFO
    }

    VkExtent2D VulkanSwapChain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actualExtent = { 960, 540 };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }
}