#include "VulkanSwapChain.hpp"

#include <algorithm>
#include <array>

#include "VulkanDevice.hpp"
#include "VulkanContext.hpp"

namespace crisp
{
    VulkanSwapChain::VulkanSwapChain(VulkanDevice* device, bool tripleBuffering)
        : VulkanResource(device)
        , m_tripleBuffering(tripleBuffering)
    {
        createSwapChain();
        createSwapChainImageViews();
    }

    VulkanSwapChain::~VulkanSwapChain()
    {
        for (auto imageView : m_imageViews)
        {
            m_device->deferDestruction(m_framesToLive, imageView, [](void* handle, VkDevice device)
            {
                vkDestroyImageView(device, static_cast<VkImageView>(handle), nullptr);
            });
        }

        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VkDevice device)
        {
            vkDestroySwapchainKHR(device, static_cast<VkSwapchainKHR>(handle), nullptr);
        });
    }

    VkFormat VulkanSwapChain::getImageFormat() const
    {
        return m_imageFormat;
    }

    VkExtent2D VulkanSwapChain::getExtent() const
    {
        return m_extent;
    }

    VkViewport VulkanSwapChain::createViewport(float minDepth, float maxDepth) const
    {
        return { 0.0f, 0.0f, static_cast<float>(m_extent.width), static_cast<float>(m_extent.height), minDepth, maxDepth };
    }

    VkRect2D VulkanSwapChain::createScissor() const
    {
        return { { 0, 0 }, m_extent };
    }

    VkImageView VulkanSwapChain::getImageView(size_t index) const
    {
        return m_imageViews.at(index);
    }

    uint32_t VulkanSwapChain::getSwapChainImageCount() const
    {
        return static_cast<uint32_t>(m_imageViews.size());
    }

    void VulkanSwapChain::recreate()
    {
        for (auto imageView : m_imageViews)
            vkDestroyImageView(m_device->getHandle(), imageView, nullptr);
        createSwapChain();
        createSwapChainImageViews();
    }

    void VulkanSwapChain::createSwapChain()
    {
        VulkanContext* context = m_device->getContext();
        VulkanSwapChainSupportDetails swapChainSupport = context->queryVulkanSwapChainSupport();

        VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode     = choosePresentMode(swapChainSupport.presentModes, m_tripleBuffering ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR);
        VkExtent2D extent                = chooseExtent(swapChainSupport.capabilities);

        uint32_t imageCount = m_tripleBuffering ? 3 : 2;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface          = context->getSurface();
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = surfaceFormat.format;
        createInfo.imageColorSpace  = surfaceFormat.colorSpace;
        createInfo.imageExtent      = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Render directly to swap chain image

        QueueFamilyIndices indices = context->findQueueFamilies();
        std::array<uint32_t, 2> queueFamilyIndices =
        {
            static_cast<uint32_t>(indices.graphicsFamily),
            static_cast<uint32_t>(indices.presentFamily)
        };
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices   = nullptr;
        }

        createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode    = VK_PRESENT_MODE_MAILBOX_KHR;
        createInfo.clipped        = VK_TRUE;
        createInfo.oldSwapchain   = m_handle;

        VkDevice deviceHandle = m_device->getHandle();
        vkCreateSwapchainKHR(deviceHandle, &createInfo, nullptr, &m_handle);

        if (createInfo.oldSwapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(deviceHandle, createInfo.oldSwapchain, nullptr);

        vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, nullptr);
        m_images.resize(imageCount);
        vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, m_images.data());

        m_imageFormat = surfaceFormat.format;
        m_extent      = extent;
    }

    void VulkanSwapChain::createSwapChainImageViews()
    {
        m_imageViews.resize(m_images.size(), VK_NULL_HANDLE);

        for (uint32_t i = 0; i < m_images.size(); i++)
        {
            VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image        = m_images[i];
            viewInfo.viewType     = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format       = m_imageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount     = 1;

            vkCreateImageView(m_device->getHandle(), &viewInfo, nullptr, &m_imageViews[i]);
        }
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

    VkPresentModeKHR VulkanSwapChain::choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR requestedPresentMode) const
    {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
        for (const auto& availablePresentMode : availablePresentModes)
            if (availablePresentMode == requestedPresentMode)
                return availablePresentMode;

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

            actualExtent.width  = std::max(capabilities.minImageExtent.width,  std::min(capabilities.maxImageExtent.width,  actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }
}