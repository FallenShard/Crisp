#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

#define STRINGIFY(x) #x

#define APPEND_FLAG_BIT_STR(res, flags, bit)                                                                           \
    if ((flags) & VK_IMAGE_USAGE_##bit) {                                                                              \
        (res) += STRINGIFY(VK_IMAGE_USAGE_##bit);                                                                      \
        (res) += " | ";                                                                                                \
    }

template <>
struct fmt::formatter<VkImageSubresourceRange> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const VkImageSubresourceRange& range, FormatContext& ctx) {
        return fmt::format_to(
            ctx.out(),
            "[Layers: [{}, {}], Mips: [{}, {}]]",
            range.baseArrayLayer,
            range.layerCount,
            range.baseMipLevel,
            range.levelCount);
    }
};

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
        CRISP_LOGF("Unknown layout encountered: {}.", static_cast<uint32_t>(layout));
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

VulkanImage::VulkanImage(const VulkanDevice& device, const VulkanImageDescription& imageDescription)
    : VulkanResource(device.getResourceDeallocator())
    , m_imageType(imageDescription.imageType)
    , m_extent(imageDescription.extent)
    , m_mipLevelCount(imageDescription.mipLevelCount)
    , m_layerCount(imageDescription.layerCount)
    , m_format(imageDescription.format)
    , m_sampleCount(imageDescription.sampleCount)
    , m_aspect(imageDescription.aspectMask.value_or(determineImageAspect(imageDescription.format)))
    , m_layouts(m_layerCount * m_mipLevelCount, VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = imageDescription.createFlags;
    createInfo.imageType = m_imageType;
    createInfo.format = m_format;
    createInfo.extent = m_extent;
    createInfo.mipLevels = m_mipLevelCount;
    createInfo.arrayLayers = m_layerCount;
    createInfo.samples = m_sampleCount;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = imageDescription.usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(
        device.getMemoryAllocator(), &createInfo, &allocInfo, &m_handle, &m_allocation, &m_allocationInfo));
}

VulkanImage::VulkanImage(const VulkanDevice& device, const VkImageCreateInfo& createInfo)
    : VulkanResource(device.getResourceDeallocator())
    , m_imageType(createInfo.imageType)
    , m_extent(createInfo.extent)
    , m_mipLevelCount(createInfo.mipLevels)
    , m_layerCount(createInfo.arrayLayers)
    , m_format(createInfo.format)
    , m_sampleCount(createInfo.samples)
    , m_aspect(determineImageAspect(createInfo.format))
    , m_layouts(m_layerCount * m_mipLevelCount, VK_IMAGE_LAYOUT_UNDEFINED) {

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(
        device.getMemoryAllocator(), &createInfo, &allocInfo, &m_handle, &m_allocation, &m_allocationInfo));
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
    , m_imageType(extent.depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D)
    , m_extent(extent)
    , m_mipLevelCount(numMipmaps)
    , m_layerCount(numLayers)
    , m_format(format)
    , m_sampleCount(VK_SAMPLE_COUNT_1_BIT)
    , m_aspect(determineImageAspect(format))
    , m_layouts(m_layerCount * m_mipLevelCount, VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = createFlags;
    createInfo.imageType = m_imageType;
    createInfo.format = m_format;
    createInfo.extent = m_extent;
    createInfo.mipLevels = m_mipLevelCount;
    createInfo.arrayLayers = m_layerCount;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(
        device.getMemoryAllocator(), &createInfo, &allocInfo, &m_handle, &m_allocation, &m_allocationInfo));
}

VulkanImage::~VulkanImage() {
    m_deallocator->deferMemoryDeallocation(kRendererVirtualFrameCount, m_allocation);
}

void VulkanImage::setImageLayout(VkImageLayout newLayout, VkImageSubresourceRange range) {
    adaptSubresouceRange(m_imageType, range);
    // CRISP_LOGI(
    //     "Transitioned {}: {} - {} from {} to {}",
    //     m_deallocator->getTag(m_handle),
    //     static_cast<void*>(m_handle),
    //     range,
    //     toString(m_layouts[range.baseArrayLayer][range.baseMipLevel]),
    //     toString(newLayout));
    CRISP_CHECK_LE(range.baseArrayLayer + range.layerCount, m_layerCount);
    CRISP_CHECK_LE(range.baseMipLevel + range.levelCount, m_mipLevelCount);
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            m_layouts[i * m_mipLevelCount + j] = newLayout;
        }
    }
}

void VulkanImage::transitionLayout(
    const VkCommandBuffer cmdBuffer,
    const VkImageLayout newLayout,
    const uint32_t baseLayer,
    const uint32_t numLayers,
    const VulkanSynchronizationScope& scope) {
    const VkImageSubresourceRange subresRange{
        .aspectMask = m_aspect,
        .baseMipLevel = 0,
        .levelCount = m_mipLevelCount,
        .baseArrayLayer = baseLayer,
        .layerCount = numLayers};
    transitionLayout(cmdBuffer, newLayout, subresRange, scope);
}

void VulkanImage::transitionLayout(
    VkCommandBuffer buffer,
    VkImageLayout newLayout,
    uint32_t baseLayer,
    uint32_t numLayers,
    uint32_t baseLevel,
    uint32_t levelCount,
    const VulkanSynchronizationScope& scope) {
    VkImageSubresourceRange subresRange{
        .aspectMask = m_aspect,
        .baseMipLevel = baseLevel,
        .levelCount = levelCount,
        .baseArrayLayer = baseLayer,
        .layerCount = numLayers};
    transitionLayout(buffer, newLayout, subresRange, scope);
}

void VulkanImage::transitionLayout(
    VkCommandBuffer cmdBuffer,
    VkImageLayout newLayout,
    VkImageSubresourceRange subresRange,
    const VulkanSynchronizationScope& scope) {
    subresRange.baseArrayLayer = m_imageType == VK_IMAGE_TYPE_3D ? 0 : subresRange.baseArrayLayer;
    subresRange.layerCount = m_imageType == VK_IMAGE_TYPE_3D ? 1 : subresRange.layerCount;

    if (matchesLayout(newLayout, subresRange)) {
        // CRISP_LOGI(
        //     "Matching {} : {} - {} from {} to {}",
        //     m_deallocator->getTag(m_handle),
        //     static_cast<void*>(m_handle),
        //     subresRange,
        //     toString(m_layouts[subresRange.baseArrayLayer][subresRange.baseMipLevel]),
        //     toString(newLayout));
        return;
    }

    CRISP_CHECK(isSameLayoutInRange(subresRange), "Attempting to transition an image across different layouts!");

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = getLayout(subresRange.baseArrayLayer, subresRange.baseMipLevel);
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_handle;
    barrier.subresourceRange = subresRange;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmdBuffer, &info);
    setImageLayout(newLayout, subresRange);
}

void VulkanImage::transitionLayout(
    const VkCommandBuffer cmdBuffer, const VkImageLayout newLayout, const VulkanSynchronizationScope& scope) {
    const auto subresRange = getFullRange();
    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = getLayout(subresRange.baseArrayLayer, subresRange.baseMipLevel);
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_handle;
    barrier.subresourceRange = subresRange;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.imageMemoryBarrierCount = 1;
    info.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmdBuffer, &info);
    setImageLayout(newLayout, subresRange);
}

void VulkanImage::copyFrom(VkCommandBuffer commandBuffer, const VulkanBuffer& buffer) {
    copyFrom(commandBuffer, buffer, m_extent, 0, m_layerCount, 0);
}

void VulkanImage::copyFrom(
    VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers) {
    copyFrom(commandBuffer, buffer, m_extent, baseLayer, numLayers, 0);
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
    vkCmdCopyBufferToImage(commandBuffer, buffer.getHandle(), m_handle, getLayout(baseLayer, mipLevel), 1, &copyRegion);
}

void VulkanImage::copyTo(
    VkCommandBuffer commandBuffer, const VulkanBuffer& buffer, uint32_t baseLayer, uint32_t numLayers) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.baseArrayLayer = baseLayer;
    region.imageSubresource.layerCount = numLayers;
    region.imageSubresource.mipLevel = 0;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = m_extent;
    vkCmdCopyImageToBuffer(commandBuffer, m_handle, getLayout(baseLayer, 0), buffer.getHandle(), 1, &region);
}

void VulkanImage::buildMipmaps(VkCommandBuffer commandBuffer, const VulkanSynchronizationStage& stage) {
    if (m_mipLevelCount > 1) {
        VkImageSubresourceRange currSubresource = {};
        currSubresource.aspectMask = m_aspect;
        currSubresource.baseMipLevel = 0;
        currSubresource.levelCount = 1;
        currSubresource.baseArrayLayer = 0;
        currSubresource.layerCount = m_layerCount;

        transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currSubresource, stage >> kTransferRead);

        for (uint32_t i = 1; i < m_mipLevelCount; i++) {
            VkImageBlit imageBlit = {};

            imageBlit.srcSubresource.aspectMask = m_aspect;
            imageBlit.srcSubresource.baseArrayLayer = 0;
            imageBlit.srcSubresource.layerCount = m_layerCount;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = std::max(static_cast<int32_t>(m_extent.width >> (i - 1)), 1);
            imageBlit.srcOffsets[1].y = std::max(static_cast<int32_t>(m_extent.height >> (i - 1)), 1);
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = m_aspect;
            imageBlit.dstSubresource.baseArrayLayer = 0;
            imageBlit.dstSubresource.layerCount = m_layerCount;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = std::max(static_cast<int32_t>(m_extent.width >> i), 1);
            imageBlit.dstOffsets[1].y = std::max(static_cast<int32_t>(m_extent.height >> i), 1);
            imageBlit.dstOffsets[1].z = 1;

            currSubresource.baseMipLevel = i;

            transitionLayout(
                commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, currSubresource, stage >> kTransferWrite);
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
                commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currSubresource, kTransferWrite >> kTransferRead);
        }
    }
}

void VulkanImage::blit(VkCommandBuffer commandBuffer, const VulkanImage& image, uint32_t mipLevel) {
    VkImageBlit imageBlit = {};
    imageBlit.srcSubresource.aspectMask = m_aspect;
    imageBlit.srcSubresource.baseArrayLayer = 0;
    imageBlit.srcSubresource.layerCount = 6;
    imageBlit.srcSubresource.mipLevel = 0;
    imageBlit.srcOffsets[1].x = static_cast<int32_t>(image.m_extent.width);
    imageBlit.srcOffsets[1].y = static_cast<int32_t>(image.m_extent.height);
    imageBlit.srcOffsets[1].z = 1;

    imageBlit.dstSubresource.aspectMask = m_aspect;
    imageBlit.dstSubresource.baseArrayLayer = 0;
    imageBlit.dstSubresource.layerCount = 6;
    imageBlit.dstSubresource.mipLevel = mipLevel;
    imageBlit.dstOffsets[1].x = static_cast<int32_t>(image.m_extent.width);
    imageBlit.dstOffsets[1].y = static_cast<int32_t>(image.m_extent.height);
    imageBlit.dstOffsets[1].z = 1;

    VkImageSubresourceRange mipRange = {};
    mipRange.aspectMask = m_aspect;
    mipRange.baseMipLevel = mipLevel;
    mipRange.levelCount = 1;
    mipRange.baseArrayLayer = 0;
    mipRange.layerCount = 6;

    transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipRange, kTransferRead >> kTransferWrite);
    vkCmdBlitImage(
        commandBuffer,
        image.getHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageBlit,
        VK_FILTER_LINEAR);
    transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipRange, kTransferWrite >> kFragmentRead);
}

VkImageSubresourceRange VulkanImage::getFirstMipRange() const {
    return {
        .aspectMask = m_aspect,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = m_layerCount,
    };
}

VkExtent2D VulkanImage::getExtent2D() const {
    return {m_extent.width, m_extent.height};
}

uint32_t VulkanImage::getMipLevels() const {
    return m_mipLevelCount;
}

uint32_t VulkanImage::getWidth() const {
    return m_extent.width;
}

uint32_t VulkanImage::getHeight() const {
    return m_extent.height;
}

VkImageAspectFlags VulkanImage::getAspectMask() const {
    return m_aspect;
}

VkFormat VulkanImage::getFormat() const {
    return m_format;
}

uint32_t VulkanImage::getLayerCount() const {
    return m_layerCount;
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

VkImageLayout VulkanImage::getLayout() const {
    return getLayout(0, 0);
}

bool VulkanImage::matchesLayout(const VkImageLayout imageLayout, const VkImageSubresourceRange& range) const {
    CRISP_CHECK_LE(range.baseArrayLayer + range.layerCount, m_layerCount);
    CRISP_CHECK_LE(range.baseMipLevel + range.levelCount, m_mipLevelCount);
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            if (getLayoutUnchecked(i, j) != imageLayout) {
                return false;
            }
        }
    }

    return true;
}

bool VulkanImage::isSameLayoutInRange(const VkImageSubresourceRange& range) const {
    CRISP_CHECK_LE(range.baseArrayLayer + range.layerCount, m_layerCount);
    CRISP_CHECK_LE(range.baseMipLevel + range.levelCount, m_mipLevelCount);

    FlatHashSet<VkImageLayout> uniqueLayouts{};
    for (uint32_t i = range.baseArrayLayer; i < range.baseArrayLayer + range.layerCount; ++i) {
        for (uint32_t j = range.baseMipLevel; j < range.baseMipLevel + range.levelCount; ++j) {
            uniqueLayouts.insert(getLayoutUnchecked(i, j));
        }
    }

    return uniqueLayouts.size() <= 1;
}

VkImageLayout VulkanImage::getLayout(const uint32_t layer, const uint32_t mipLevel) const {
    CRISP_CHECK_LT(layer, m_layerCount);
    CRISP_CHECK_LT(mipLevel, m_mipLevelCount);
    return m_layouts[layer * m_mipLevelCount + mipLevel];
}

VkImageLayout VulkanImage::getLayoutUnchecked(const uint32_t layer, const uint32_t mipLevel) const {
    return m_layouts[layer * m_mipLevelCount + mipLevel];
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