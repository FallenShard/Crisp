#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

#include <algorithm>

namespace crisp {

VulkanCommandEncoder::VulkanCommandEncoder(const VkCommandBuffer cmdBuffer)
    : m_cmdBuffer(cmdBuffer) {}

void VulkanCommandEncoder::setViewport(const VkViewport& viewport) const {
    vkCmdSetViewport(m_cmdBuffer, 0, 1, &viewport);
}

void VulkanCommandEncoder::setScissor(const VkRect2D& scissorRect) const {
    vkCmdSetScissor(m_cmdBuffer, 0, 1, &scissorRect);
}

void VulkanCommandEncoder::bindPipeline(const VulkanPipeline& pipeline) const {
    vkCmdBindPipeline(m_cmdBuffer, pipeline.getBindPoint(), pipeline.getHandle());
}

void VulkanCommandEncoder::insertBarrier(const VulkanSynchronizationScope& scope) const {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    info.memoryBarrierCount = 1;
    info.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::insertBufferMemoryBarrier(
    const VkBuffer buffer,
    const VkDeviceSize offset,
    const VkDeviceSize size,
    const VulkanSynchronizationScope& scope) const {
    VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::insertBufferMemoryBarriers(
    const std::span<const VkBufferMemoryBarrier2> barriers) const {
    const VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pBufferMemoryBarriers = barriers.data(),
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::insertImageMemoryBarrier(const VkImageMemoryBarrier2& barrier) const {
    const VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::transferBufferOwnership(
    const VkBuffer buffer,
    const uint32_t srcQueueFamilyIndex,
    const uint32_t dstQueueFamilyIndex,
    const VulkanSynchronizationScope& scope) const {
    VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;

    insertBufferMemoryBarriers(std::span{&barrier, 1});
}

void VulkanCommandEncoder::copyBuffer(
    const VkBuffer src, const VkBuffer dst, const std::span<const VkBufferCopy> regions) const {
    vkCmdCopyBuffer(m_cmdBuffer, src, dst, static_cast<uint32_t>(regions.size()), regions.data());
}

void VulkanCommandEncoder::copyBuffer(
    const VulkanBuffer& src, const VulkanBuffer& dst, const VkBufferCopy& region) const {
    copyBuffer(src.getHandle(), dst.getHandle(), std::span{&region, 1});
}

void VulkanCommandEncoder::copyBuffer(const VulkanBuffer& src, const VulkanBuffer& dst) const {
    CRISP_CHECK_LE(dst.getSize(), src.getSize());
    const VkBufferCopy region{.size = dst.getSize()};
    copyBuffer(src, dst, region);
}

void VulkanCommandEncoder::transitionLayout(
    VulkanImage& image, const VkImageLayout newLayout, const VulkanSynchronizationScope& scope) const {
    const auto range = image.getFullRange();
    transitionLayout(image, newLayout, scope, range);
}

void VulkanCommandEncoder::transitionLayout(
    VulkanImage& image,
    const VkImageLayout newLayout,
    const VulkanSynchronizationScope& scope,
    const VkImageSubresourceRange& range) const {
    CRISP_CHECK(image.isSameLayoutInRange(range), "Attempting to transition an image across different layouts!");

    VkImageMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.oldLayout = image.getLayout(range.baseArrayLayer, range.baseMipLevel);
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.getHandle();
    barrier.subresourceRange = range;
    barrier.srcStageMask = scope.srcStage;
    barrier.srcAccessMask = scope.srcAccess;
    barrier.dstStageMask = scope.dstStage;
    barrier.dstAccessMask = scope.dstAccess;

    const VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
    image.setImageLayout(newLayout, range);
}

void VulkanCommandEncoder::transitionLayout(
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const VulkanSynchronizationScope& scope,
    const VkImageSubresourceRange& range) const {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = scope.srcStage,
        .srcAccessMask = scope.srcAccess,
        .dstStageMask = scope.dstStage,
        .dstAccessMask = scope.dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range,
    };
    const VkDependencyInfo info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(m_cmdBuffer, &info);
}

void VulkanCommandEncoder::beginRendering(const VkRenderingInfo& renderingInfo) const {
    vkCmdBeginRendering(m_cmdBuffer, &renderingInfo);
}

void VulkanCommandEncoder::endRendering() const {
    vkCmdEndRendering(m_cmdBuffer);
}

void VulkanCommandEncoder::copyBufferToImage(
    const VkBuffer src, VulkanImage& dst, const std::span<const VkBufferImageCopy> regions) const {
    CRISP_CHECK(!regions.empty());
    const auto& subresource = regions.front().imageSubresource;
    vkCmdCopyBufferToImage(
        m_cmdBuffer,
        src,
        dst.getHandle(),
        dst.getLayout(subresource.baseArrayLayer, subresource.mipLevel),
        static_cast<uint32_t>(regions.size()),
        regions.data());
}

void VulkanCommandEncoder::copyBufferToImage(
    const VulkanBuffer& src, VulkanImage& dst, const VkBufferImageCopy& region) const {
    copyBufferToImage(src.getHandle(), dst, std::span{&region, 1});
}

void VulkanCommandEncoder::copyImageToBuffer(
    const VulkanImage& src, const VkBuffer dst, const std::span<const VkBufferImageCopy> regions) const {
    CRISP_CHECK(!regions.empty());
    const auto& subresource = regions.front().imageSubresource;
    vkCmdCopyImageToBuffer(
        m_cmdBuffer,
        src.getHandle(),
        src.getLayout(subresource.baseArrayLayer, subresource.mipLevel),
        dst,
        static_cast<uint32_t>(regions.size()),
        regions.data());
}

void VulkanCommandEncoder::copyImageToBuffer(
    const VulkanImage& src, const VulkanBuffer& dst, const VkBufferImageCopy& region) const {
    copyImageToBuffer(src, dst.getHandle(), std::span{&region, 1});
}

void VulkanCommandEncoder::blitImage(
    const VulkanImage& src, VulkanImage& dst, const VkImageBlit& region, const VkFilter filter) const {
    vkCmdBlitImage(
        m_cmdBuffer,
        src.getHandle(),
        src.getLayout(region.srcSubresource.baseArrayLayer, region.srcSubresource.mipLevel),
        dst.getHandle(),
        dst.getLayout(region.dstSubresource.baseArrayLayer, region.dstSubresource.mipLevel),
        1,
        &region,
        filter);
}

void VulkanCommandEncoder::generateMipmaps(
    VulkanImage& image, const VulkanSynchronizationStage& initialStage) const {
    if (image.getMipLevels() <= 1) {
        return;
    }

    VkImageSubresourceRange mipRange{
        .aspectMask = image.getAspectMask(),
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = image.getLayerCount(),
    };
    transitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, initialStage >> kTransferRead, mipRange);

    for (uint32_t mipLevel = 1; mipLevel < image.getMipLevels(); ++mipLevel) {
        VkImageBlit region{};
        region.srcSubresource = {
            .aspectMask = image.getAspectMask(),
            .mipLevel = mipLevel - 1,
            .baseArrayLayer = 0,
            .layerCount = image.getLayerCount(),
        };
        region.srcOffsets[1] = {
            std::max(static_cast<int32_t>(image.getWidth() >> (mipLevel - 1)), 1),
            std::max(static_cast<int32_t>(image.getHeight() >> (mipLevel - 1)), 1),
            1,
        };
        region.dstSubresource = {
            .aspectMask = image.getAspectMask(),
            .mipLevel = mipLevel,
            .baseArrayLayer = 0,
            .layerCount = image.getLayerCount(),
        };
        region.dstOffsets[1] = {
            std::max(static_cast<int32_t>(image.getWidth() >> mipLevel), 1),
            std::max(static_cast<int32_t>(image.getHeight() >> mipLevel), 1),
            1,
        };

        mipRange.baseMipLevel = mipLevel;
        transitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, initialStage >> kTransferWrite, mipRange);
        blitImage(image, image, region);
        transitionLayout(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, kTransferWrite >> kTransferRead, mipRange);
    }
}

void VulkanCommandEncoder::dispatchCompute(const VkExtent3D& workGroupCount) const {
    vkCmdDispatch(m_cmdBuffer, workGroupCount.width, workGroupCount.height, workGroupCount.depth);
}

void VulkanCommandEncoder::drawMeshTasks(const VkExtent3D& groupCount) const {
    vkCmdDrawMeshTasksEXT(m_cmdBuffer, groupCount.width, groupCount.height, groupCount.depth);
}

void VulkanCommandEncoder::drawMeshTasks(const uint32_t groupCount) const {
    vkCmdDrawMeshTasksEXT(m_cmdBuffer, groupCount, 1, 1);
}

void VulkanCommandEncoder::traceRays(
    const std::span<const VkStridedDeviceAddressRegionKHR> bindingRegions, const VkExtent2D& gridSize) const {
    CRISP_CHECK_GE(bindingRegions.size(), 4);
    vkCmdTraceRaysKHR(
        m_cmdBuffer,
        &bindingRegions[0], // Raygen NOLINT
        &bindingRegions[1], // Miss
        &bindingRegions[2], // Hit
        &bindingRegions[3], // Callable
        gridSize.width,
        gridSize.height,
        1);
}

} // namespace crisp
