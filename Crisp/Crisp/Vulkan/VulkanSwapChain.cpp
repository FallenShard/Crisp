#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <Crisp/Vulkan/VulkanChecks.hpp>
#include <Crisp/Vulkan/VulkanResourceDeallocator.hpp>

#include <array>

namespace crisp
{
VulkanSwapChain::VulkanSwapChain(
    const VulkanDevice& device,
    const VulkanPhysicalDevice& physicalDevice,
    const VkSurfaceKHR surface,
    const TripleBuffering tripleBuffering)
    : VulkanResource(device.getResourceDeallocator())
    , m_imageFormat{}
    , m_extent{}
    , m_presentationMode(
          tripleBuffering == TripleBuffering::Enabled ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR)
{
    createSwapChain(device, physicalDevice, surface);
    createImageViews(device.getHandle());
}

VulkanSwapChain::~VulkanSwapChain()
{
    for (auto* imageView : m_imageViews)
    {
        m_deallocator->deferDestruction(m_framesToLive, imageView);
    }
}

VulkanSwapChain::VulkanSwapChain(VulkanSwapChain&& other) noexcept
    : VulkanResource(std::move(other))
    , m_images(std::move(other.m_images))
    , m_imageViews(std::move(other.m_imageViews))
    , m_imageFormat(other.m_imageFormat)
    , m_extent(other.m_extent)
    , m_presentationMode(other.m_presentationMode)
{
}

VulkanSwapChain& VulkanSwapChain::operator=(VulkanSwapChain&& other) noexcept
{
    VulkanResource::operator=(std::move(other));
    m_images = std::move(other.m_images);
    m_imageViews = std::move(other.m_imageViews);
    m_imageFormat = other.m_imageFormat;
    m_extent = other.m_extent;
    m_presentationMode = other.m_presentationMode;
    return *this;
}

VkFormat VulkanSwapChain::getImageFormat() const
{
    return m_imageFormat;
}

VkExtent2D VulkanSwapChain::getExtent() const
{
    return m_extent;
}

VkViewport VulkanSwapChain::getViewport(float minDepth, float maxDepth) const
{
    return {0.0f, 0.0f, static_cast<float>(m_extent.width), static_cast<float>(m_extent.height), minDepth, maxDepth};
}

VkRect2D VulkanSwapChain::getScissorRect() const
{
    return {
        {0, 0},
        m_extent
    };
}

VkImageView VulkanSwapChain::getImageView(size_t index) const
{
    return m_imageViews.at(index);
}

uint32_t VulkanSwapChain::getImageCount() const
{
    return static_cast<uint32_t>(m_imageViews.size());
}

void VulkanSwapChain::recreate(
    const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice, const VkSurfaceKHR surface)
{
    for (auto* imageView : m_imageViews)
    {
        vkDestroyImageView(device.getHandle(), imageView, nullptr);
    }
    createSwapChain(device, physicalDevice, surface);
    createImageViews(device.getHandle());
}

void VulkanSwapChain::createSwapChain(
    const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice, const VkSurfaceKHR surface)
{
    const SurfaceSupport swapChainSupport = physicalDevice.querySurfaceSupport(surface);
    const VkSurfaceFormatKHR surfaceFormat =
        selectSurfaceFormat(swapChainSupport.formats, {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .unwrap();
    const VkExtent2D extent = determineExtent(swapChainSupport.capabilities);

    uint32_t imageCount = m_presentationMode == VK_PRESENT_MODE_MAILBOX_KHR ? 3 : 2;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Render directly to swap chain image

    const QueueFamilyIndices indices = physicalDevice.queryQueueFamilyIndices(surface);
    const std::array<uint32_t, 2> queueFamilyIndices = {*indices.graphicsFamily, *indices.presentFamily};
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
    createInfo.presentMode = selectPresentMode(swapChainSupport.presentModes, m_presentationMode).unwrap();
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_handle;

    VkDevice deviceHandle = device.getHandle();
    VK_CHECK(vkCreateSwapchainKHR(deviceHandle, &createInfo, nullptr, &m_handle));

    if (createInfo.oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(deviceHandle, createInfo.oldSwapchain, nullptr);
    }

    VK_CHECK(vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, nullptr));
    m_images.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, m_images.data()));

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
}

void VulkanSwapChain::createImageViews(const VkDevice deviceHandle)
{
    m_imageViews.resize(m_images.size(), VK_NULL_HANDLE);

    for (uint32_t i = 0; i < m_images.size(); i++)
    {
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_imageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(deviceHandle, &viewInfo, nullptr, &m_imageViews[i]));
    }
}

Result<VkSurfaceFormatKHR> VulkanSwapChain::selectSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats, const VkSurfaceFormatKHR& surfaceFormat)
{
    if (availableFormats.empty())
    {
        return resultError("No surface formats available for this swap chain!");
    }

    if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        return VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const auto& format : availableFormats)
    {
        if (format.format == surfaceFormat.format && format.colorSpace == surfaceFormat.colorSpace)
        {
            return format;
        }
    }

    return resultError("Requested surface format was not found!");
}

Result<VkPresentModeKHR> VulkanSwapChain::selectPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR requestedPresentMode)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == requestedPresentMode)
        {
            return availablePresentMode;
        }
    }

    return resultError("Unavailable present mode requested!");
}

VkExtent2D VulkanSwapChain::determineExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    return {
        std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, 960u)),
        std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, 540u))};
}
} // namespace crisp