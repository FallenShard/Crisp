#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>

namespace crisp {

VulkanStagingBelt::VulkanStagingBelt(VulkanDevice& device, const VkDeviceSize capacity)
    : m_device(&device)
    , m_stagingBuffer(device, capacity, device.getNonCoherentAtomSize()) {}

void VulkanStagingBelt::beginFrame(const uint32_t virtualFrameIndex) {
    CRISP_CHECK_LT(virtualFrameIndex, kMaxVirtualFrames);
    m_currentVirtualFrame = virtualFrameIndex;
    m_stagingBuffer.reclaim(m_frameWatermarks[virtualFrameIndex]);
}

void VulkanStagingBelt::endFrame() {
    m_frameWatermarks[m_currentVirtualFrame] = m_stagingBuffer.getHead();
}

StagingAllocation VulkanStagingBelt::stageData(const void* data, const VkDeviceSize size) {
    auto alloc = m_stagingBuffer.allocate(size);
    CRISP_CHECK(alloc.has_value());
    std::memcpy(alloc->mappedPtr, data, size);
    return *alloc;
}

VkBuffer VulkanStagingBelt::getStagingBufferHandle() const {
    return m_stagingBuffer.getHandle();
}

void VulkanStagingBelt::reclaimAll() {
    m_stagingBuffer.reclaimAll();
}

void VulkanStagingBelt::uploadBuffer(
    const VkCommandBuffer cmdBuffer,
    const VulkanBuffer& dstBuffer,
    const VkDeviceSize dstOffset,
    const void* data,
    const VkDeviceSize size) {
    const auto alloc = stageData(data, size);

    VkBufferCopy region{};
    region.srcOffset = alloc.offset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(cmdBuffer, m_stagingBuffer.getHandle(), dstBuffer.getHandle(), 1, &region);
}

void VulkanStagingBelt::uploadBufferBlocking(
    const VulkanQueue& queue,
    const VulkanBuffer& dstBuffer,
    const VkDeviceSize dstOffset,
    const void* data,
    const VkDeviceSize size) {
    if (size > m_stagingBuffer.getCapacity()) {
        spdlog::warn(
            "VulkanStagingBelt: upload of {} bytes exceeds ring capacity of {}. Using temporary staging buffer.",
            size,
            m_stagingBuffer.getCapacity());

        auto tempStaging = std::make_unique<VulkanBuffer>(
            *m_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BufferMemoryType::HostUpload);
        tempStaging->updateFromHost(data, size, 0);
        queue.submitAndWait([&](const VkCommandBuffer cmdBuffer) {
            dstBuffer.copyFrom(cmdBuffer, *tempStaging);
        });
        return;
    }

    const auto alloc = stageData(data, size);
    queue.submitAndWait([&](const VkCommandBuffer cmdBuffer) {
        VkBufferCopy region{};
        region.srcOffset = alloc.offset;
        region.dstOffset = dstOffset;
        region.size = size;
        vkCmdCopyBuffer(cmdBuffer, m_stagingBuffer.getHandle(), dstBuffer.getHandle(), 1, &region);
    });

    m_stagingBuffer.reclaim(m_stagingBuffer.getHead());
}

void VulkanStagingBelt::uploadBufferBlocking(
    const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const void* data, const VkDeviceSize size) {
    uploadBufferBlocking(queue, dstBuffer, 0, data, size);
}

void VulkanStagingBelt::uploadImage(
    const VkCommandBuffer cmdBuffer,
    const VulkanImage& dstImage,
    const uint32_t baseLayer,
    const uint32_t numLayers,
    const uint32_t mipLevel,
    const void* data,
    const VkDeviceSize size) {
    const auto extent2D = dstImage.getExtent2D();
    uploadImage(cmdBuffer, dstImage, {extent2D.width, extent2D.height, 1u}, baseLayer, numLayers, mipLevel, data, size);
}

void VulkanStagingBelt::uploadImage(
    const VkCommandBuffer cmdBuffer,
    const VulkanImage& dstImage,
    const VkExtent3D extent,
    const uint32_t baseLayer,
    const uint32_t numLayers,
    const uint32_t mipLevel,
    const void* data,
    const VkDeviceSize size) {
    const auto alloc = stageData(data, size);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = alloc.offset;
    copyRegion.bufferRowLength = extent.width;
    copyRegion.bufferImageHeight = extent.height;
    copyRegion.imageExtent = extent;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageSubresource.aspectMask = dstImage.getAspectMask();
    copyRegion.imageSubresource.baseArrayLayer = baseLayer;
    copyRegion.imageSubresource.layerCount = numLayers;
    copyRegion.imageSubresource.mipLevel = mipLevel;
    vkCmdCopyBufferToImage(
        cmdBuffer,
        m_stagingBuffer.getHandle(),
        dstImage.getHandle(),
        dstImage.getLayout(baseLayer, mipLevel),
        1,
        &copyRegion);
}

ReadbackBuffer VulkanStagingBelt::downloadBuffer(
    const VkCommandBuffer cmd, const VulkanBuffer& src, const VkDeviceSize srcOffset, const VkDeviceSize size) {
    auto buffer = std::make_unique<VulkanBuffer>(
        *m_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    VkBufferCopy region{};
    region.srcOffset = srcOffset;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmd, src.getHandle(), buffer->getHandle(), 1, &region);

    return {.buffer = std::move(buffer)};
}

ReadbackBuffer VulkanStagingBelt::downloadImage(
    const VkCommandBuffer cmd,
    const VulkanImage& src,
    const VkExtent3D extent,
    const uint32_t baseLayer,
    const uint32_t numLayers,
    const uint32_t mipLevel,
    const VkDeviceSize size) {
    auto buffer = std::make_unique<VulkanBuffer>(
        *m_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, BufferMemoryType::HostReadback);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageExtent = extent;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageSubresource.aspectMask = src.getAspectMask();
    copyRegion.imageSubresource.baseArrayLayer = baseLayer;
    copyRegion.imageSubresource.layerCount = numLayers;
    copyRegion.imageSubresource.mipLevel = mipLevel;
    vkCmdCopyImageToBuffer(
        cmd, src.getHandle(), src.getLayout(baseLayer, mipLevel), buffer->getHandle(), 1, &copyRegion);

    return {.buffer = std::move(buffer)};
}

VulkanStagingBuffer& VulkanStagingBelt::getStagingBuffer() {
    return m_stagingBuffer;
}

const VulkanStagingBuffer& VulkanStagingBelt::getStagingBuffer() const {
    return m_stagingBuffer;
}

} // namespace crisp
