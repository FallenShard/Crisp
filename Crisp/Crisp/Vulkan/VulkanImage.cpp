#include <Crisp/Vulkan/VulkanImage.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>

#define STRINGIFY(x) #x

#define APPEND_FLAG_BIT_STR(res, flags, bit)                                                                           \
    if ((flags) & VK_IMAGE_USAGE_##bit) {                                                                              \
        (res) += STRINGIFY(VK_IMAGE_USAGE_##bit);                                                                      \
        (res) += " | ";                                                                                                \
    }

namespace crisp {
namespace {
auto logger = createLoggerMt("VulkanImage");

void adaptSubresouceRange(VkImageType type, VkImageSubresourceRange& subresouceRange) {
    if (type == VK_IMAGE_TYPE_3D) {
        subresouceRange.baseArrayLayer = 0;
        subresouceRange.layerCount = 1;
    }
}

} // namespace

const char* toString(const VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return "Undefined";
    case VK_IMAGE_LAYOUT_GENERAL:
        return "General";
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return "ColorAttachment";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return "DepthStencilAttachment";
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return "DepthStencilReadOnly";
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return "ShaderReadOnly";
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return "TransferSrc";
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return "TransferDst";
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return "Preinitialized";
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        return "DepthReadOnlyStencilAttachment";
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        return "DepthAttachmentStencilReadOnly";
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        return "DepthAttachment";
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        return "DepthReadOnly";
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
        return "StencilAttachment";
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
        return "StencilReadOnly";
    case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
        return "ReadOnly";
    case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL:
        return "Attachment";
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return "PresentSrc";
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
        return "SharedPresent";
    case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
        return "FragmentDensityMap";
    case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
        return "FragmentShadingRateAttachment";
    default:
        logger->critical("Unknown layout encountered: {}.", static_cast<uint32_t>(layout));
        return "Unknown";
    }
}

std::string toString(VkImageUsageFlags flags) {
    std::string res;
    APPEND_FLAG_BIT_STR(res, flags, TRANSFER_SRC_BIT);
    APPEND_FLAG_BIT_STR(res, flags, TRANSFER_DST_BIT);
    APPEND_FLAG_BIT_STR(res, flags, SAMPLED_BIT);
    APPEND_FLAG_BIT_STR(res, flags, STORAGE_BIT);
    APPEND_FLAG_BIT_STR(res, flags, COLOR_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, DEPTH_STENCIL_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, TRANSIENT_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, INPUT_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, FRAGMENT_DENSITY_MAP_BIT_EXT);
    APPEND_FLAG_BIT_STR(res, flags, FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
    return res;
}

VulkanImage::VulkanImage(const VulkanDevice& device, const VkImageCreateInfo& createInfo)
    : VulkanResource(device.createImage(createInfo), device.getResourceDeallocator())
    , m_allocation{}
    , m_device(&device)
    , m_createInfo(createInfo)
    , m_aspect(determineImageAspect(m_createInfo.format))
    , m_layouts(m_createInfo.arrayLayers, std::vector<VkImageLayout>(m_createInfo.mipLevels, VK_IMAGE_LAYOUT_UNDEFINED)) {
    // Assign the image to the proper memory heap
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.getHandle(), m_handle, &memRequirements);
    m_allocation =
        device.getMemoryAllocator().allocateImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements).unwrap();
    vkBindImageMemory(device.getHandle(), m_handle, m_allocation.getMemory(), m_allocation.offset);
}

VulkanImage::VulkanImage(
    const VulkanDevice& device,
    const VkExtent3D extent,
    const uint32_t numLayers,
    const uint32_t numMipmaps,
    const VkFormat format,
    const VkImageUsageFlags usage,
    const VkImageCreateFlags createFlags)
    : VulkanResource(device.getResourceDeallocator())
    , m_allocation{}
    , m_device(&device)
    , m_createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}
    , m_aspect(determineImageAspect(format))
    , m_layouts(numLayers, std::vector<VkImageLayout>(numMipmaps, VK_IMAGE_LAYOUT_UNDEFINED)) {
    m_createInfo.flags = createFlags;
    m_createInfo.imageType = extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;
    m_createInfo.format = format;
    m_createInfo.extent = extent;
    m_createInfo.mipLevels = numMipmaps;
    m_createInfo.arrayLayers = numLayers;
    m_createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    m_createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    m_createInfo.usage = usage;
    m_createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    m_createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_handle = device.createImage(m_createInfo);

    // Assign the image to the proper memory heap
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device.getHandle(), m_handle, &memRequirements);
    m_allocation =
        device.getMemoryAllocator().allocateImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements).unwrap();
    vkBindImageMemory(device.getHandle(), m_handle, m_allocation.getMemory(), m_allocation.offset);
}

VulkanImage::~VulkanImage() {
    m_deallocator->deferMemoryDeallocation(m_framesToLive, m_allocation);
}

void VulkanImage::setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange range) {
    adaptSubresouceRange(m_createInfo.imageType, range);
    CRISP_CHECK(range.baseArrayLayer >= 0 && range.baseArrayLayer + range.layerCount <= m_layouts.size());
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        CRISP_CHECK(range.baseMipLevel >= 0 && range.baseMipLevel + range.levelCount <= m_layouts[i].size());
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            m_layouts[i][j] = newLayout;
        }
    }
}

void VulkanImage::transitionLayout(
    VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageSubresourceRange subresRange{
        .aspectMask = m_aspect,
        .baseMipLevel = 0,
        .levelCount = m_createInfo.mipLevels,
        .baseArrayLayer = 0,
        .layerCount = m_createInfo.arrayLayers};
    transitionLayout(cmdBuffer, newLayout, subresRange, srcStage, dstStage);
}

void VulkanImage::transitionLayout(
    VkCommandBuffer cmdBuffer,
    VkImageLayout newLayout,
    uint32_t baseLayer,
    uint32_t numLayers,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) {
    VkImageSubresourceRange subresRange{
        .aspectMask = m_aspect,
        .baseMipLevel = 0,
        .levelCount = m_createInfo.mipLevels,
        .baseArrayLayer = baseLayer,
        .layerCount = numLayers};
    transitionLayout(cmdBuffer, newLayout, subresRange, srcStage, dstStage);
}

void VulkanImage::transitionLayout(
    VkCommandBuffer buffer,
    VkImageLayout newLayout,
    uint32_t baseLayer,
    uint32_t numLayers,
    uint32_t baseLevel,
    uint32_t levelCount,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) {
    VkImageSubresourceRange subresRange{
        .aspectMask = m_aspect,
        .baseMipLevel = baseLevel,
        .levelCount = levelCount,
        .baseArrayLayer = baseLayer,
        .layerCount = numLayers};
    transitionLayout(buffer, newLayout, subresRange, srcStage, dstStage);
}

void VulkanImage::transitionLayout(
    VkCommandBuffer cmdBuffer,
    VkImageLayout newLayout,
    VkImageSubresourceRange subresRange,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) {
    subresRange.baseArrayLayer = m_createInfo.imageType == VK_IMAGE_TYPE_3D ? 0 : subresRange.baseArrayLayer;
    subresRange.layerCount = m_createInfo.imageType == VK_IMAGE_TYPE_3D ? 1 : subresRange.layerCount;

    if (matchesLayout(newLayout, subresRange)) {
        return;
    }

    CRISP_CHECK(isSameLayoutInRange(subresRange), "Attempting to transition an image across different layouts!");

    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = m_layouts[subresRange.baseArrayLayer][subresRange.baseMipLevel];
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_handle;
    barrier.subresourceRange = subresRange;
    std::tie(barrier.srcAccessMask, barrier.dstAccessMask) =
        determineAccessMasks(m_layouts[subresRange.baseArrayLayer][subresRange.baseMipLevel], newLayout);

    vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    setImageLayout(newLayout, subresRange);
}

void VulkanImage::copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer) {
    copyFrom(commandBuffer, buffer, 0, m_createInfo.arrayLayers);
}

void VulkanImage::copyFrom(
    VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers) {
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferImageHeight = m_createInfo.extent.height;
    copyRegion.bufferRowLength = m_createInfo.extent.width;
    copyRegion.imageExtent = m_createInfo.extent;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageSubresource.aspectMask = m_aspect;
    copyRegion.imageSubresource.baseArrayLayer = baseLayer;
    copyRegion.imageSubresource.layerCount = numLayers;
    copyRegion.imageSubresource.mipLevel = 0;
    vkCmdCopyBufferToImage(commandBuffer, buffer.getHandle(), m_handle, m_layouts[baseLayer][0], 1, &copyRegion);
}

void VulkanImage::copyFrom(
    VkCommandBuffer commandBuffer,
    const VulkanBuffer& buffer,
    const VkExtent3D& extent,
    const uint32_t baseLayer,
    const uint32_t numLayers,
    const uint32_t mipLevel) {
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferImageHeight = extent.height;
    copyRegion.bufferRowLength = extent.width;
    copyRegion.imageExtent = extent;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageSubresource.aspectMask = m_aspect;
    copyRegion.imageSubresource.baseArrayLayer = baseLayer;
    copyRegion.imageSubresource.layerCount = numLayers;
    copyRegion.imageSubresource.mipLevel = mipLevel;
    vkCmdCopyBufferToImage(commandBuffer, buffer.getHandle(), m_handle, m_layouts[baseLayer][mipLevel], 1, &copyRegion);
}

void VulkanImage::buildMipmaps(VkCommandBuffer commandBuffer) {
    if (m_createInfo.mipLevels > 1) {
        VkImageSubresourceRange currSubresource = {};
        currSubresource.aspectMask = m_aspect;
        currSubresource.baseMipLevel = 0;
        currSubresource.levelCount = 1;
        currSubresource.baseArrayLayer = 0;
        currSubresource.layerCount = m_createInfo.arrayLayers;

        transitionLayout(
            commandBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            currSubresource,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        for (uint32_t i = 1; i < m_createInfo.mipLevels; i++) {
            VkImageBlit imageBlit = {};

            imageBlit.srcSubresource.aspectMask = m_aspect;
            imageBlit.srcSubresource.baseArrayLayer = 0;
            imageBlit.srcSubresource.layerCount = m_createInfo.arrayLayers;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = std::max(static_cast<int32_t>(m_createInfo.extent.width >> (i - 1)), 1);
            imageBlit.srcOffsets[1].y = std::max(static_cast<int32_t>(m_createInfo.extent.height >> (i - 1)), 1);
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = m_aspect;
            imageBlit.dstSubresource.baseArrayLayer = 0;
            imageBlit.dstSubresource.layerCount = m_createInfo.arrayLayers;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = std::max(static_cast<int32_t>(m_createInfo.extent.width >> i), 1);
            imageBlit.dstOffsets[1].y = std::max(static_cast<int32_t>(m_createInfo.extent.height >> i), 1);
            imageBlit.dstOffsets[1].z = 1;

            currSubresource.baseMipLevel = i;

            transitionLayout(
                commandBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                currSubresource,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
            vkCmdBlitImage(
                commandBuffer,
                m_handle,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &imageBlit,
                VK_FILTER_LINEAR);
            transitionLayout(
                commandBuffer,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                currSubresource,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        }
    }
}

void VulkanImage::blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel) {
    VkImageBlit imageBlit = {};
    imageBlit.srcSubresource.aspectMask = m_aspect;
    imageBlit.srcSubresource.baseArrayLayer = 0;
    imageBlit.srcSubresource.layerCount = 6;
    imageBlit.srcSubresource.mipLevel = 0;
    imageBlit.srcOffsets[1].x = static_cast<int32_t>(image.m_createInfo.extent.width);
    imageBlit.srcOffsets[1].y = static_cast<int32_t>(image.m_createInfo.extent.height);
    imageBlit.srcOffsets[1].z = 1;

    imageBlit.dstSubresource.aspectMask = m_aspect;
    imageBlit.dstSubresource.baseArrayLayer = 0;
    imageBlit.dstSubresource.layerCount = 6;
    imageBlit.dstSubresource.mipLevel = mipLevel;
    imageBlit.dstOffsets[1].x = static_cast<int32_t>(image.m_createInfo.extent.width);
    imageBlit.dstOffsets[1].y = static_cast<int32_t>(image.m_createInfo.extent.height);
    imageBlit.dstOffsets[1].z = 1;

    VkImageSubresourceRange mipRange = {};
    mipRange.aspectMask = m_aspect;
    mipRange.baseMipLevel = mipLevel;
    mipRange.levelCount = 1;
    mipRange.baseArrayLayer = 0;
    mipRange.layerCount = 6;

    transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mipRange,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    vkCmdBlitImage(
        commandBuffer,
        image.getHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageBlit,
        VK_FILTER_LINEAR);
    transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mipRange,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

uint32_t VulkanImage::getMipLevels() const {
    return m_createInfo.mipLevels;
}

uint32_t VulkanImage::getWidth() const {
    return m_createInfo.extent.width;
}

uint32_t VulkanImage::getHeight() const {
    return m_createInfo.extent.height;
}

VkImageAspectFlags VulkanImage::getAspectMask() const {
    return m_aspect;
}

VkFormat VulkanImage::getFormat() const {
    return m_createInfo.format;
}

uint32_t VulkanImage::getLayerCount() const {
    return m_createInfo.arrayLayers;
}

VkImageSubresourceRange VulkanImage::getFullRange() const {
    return {
        .aspectMask = getAspectMask(),
        .baseMipLevel = 0,
        .levelCount = getMipLevels(),
        .baseArrayLayer = 0,
        .layerCount = getLayerCount(),
    };
}

const VulkanDevice& VulkanImage::getDevice() const {
    return *m_device;
}

bool VulkanImage::matchesLayout(VkImageLayout imageLayout, const VkImageSubresourceRange& range) const {
    CRISP_CHECK(range.baseArrayLayer >= 0 && range.baseArrayLayer + range.layerCount <= m_layouts.size());
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        CRISP_CHECK(range.baseMipLevel >= 0 && range.baseMipLevel + range.levelCount <= m_layouts[i].size());
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            if (m_layouts[i][j] != imageLayout) {
                return false;
            }
        }
    }

    return true;
}

bool VulkanImage::isSameLayoutInRange(const VkImageSubresourceRange& range) const {
    CRISP_CHECK(range.baseArrayLayer >= 0 && range.baseArrayLayer + range.layerCount <= m_layouts.size());

    FlatHashSet<VkImageLayout> uniqueLayouts{};
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        CRISP_CHECK(range.baseMipLevel >= 0 && range.baseMipLevel + range.levelCount <= m_layouts[i].size());
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            uniqueLayouts.insert(m_layouts[i][j]);
        }
    }

    return uniqueLayouts.size() <= 1;
}

std::pair<VkAccessFlags, VkAccessFlags> VulkanImage::determineAccessMasks(
    VkImageLayout oldLayout, VkImageLayout newLayout) const {
    switch (oldLayout) {
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
        }

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL:
            return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        }

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT};
        }

    case VK_IMAGE_LAYOUT_UNDEFINED:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {0, VK_ACCESS_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return {0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return {0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {0, VK_ACCESS_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL:
            return {0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
        }

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT};
        case VK_IMAGE_LAYOUT_GENERAL:
            return {VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT};
        }

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return {VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
        }

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        }

    case VK_IMAGE_LAYOUT_GENERAL:
        switch (newLayout) {
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return {VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return {VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};
        }
    }

    logger->critical("Unsupported layout transition: {} to {}!", toString(oldLayout), toString(newLayout));
    std::exit(-1);
}

VkImageAspectFlags determineImageAspect(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

bool isDepthFormat(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}
} // namespace crisp