#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>

#include <algorithm>

namespace crisp {

VulkanStagingBelt::VulkanStagingBelt(VulkanDevice& device, const VkDeviceSize initialCapacity)
    : m_device(&device)
    , m_initialCapacity(initialCapacity)
    , m_alignment(device.getNonCoherentAtomSize()) {
    addChunk(m_initialCapacity);
}

VulkanStagingBelt::Chunk& VulkanStagingBelt::addChunk(const VkDeviceSize capacity) {
    return m_chunks.emplace_back(Chunk{
        .buffer = VulkanStagingBuffer(*m_device, capacity, m_alignment),
        .lastUsedFrame = m_currentFrame,
    });
}

void VulkanStagingBelt::beginFrame(const uint32_t virtualFrameIndex) {
    CRISP_CHECK_LT(
        virtualFrameIndex,
        kMaxVirtualFrames,
        "virtualFrameIndex={} exceeds belt's kMaxVirtualFrames={}",
        virtualFrameIndex,
        kMaxVirtualFrames);
    ++m_currentFrame;
    m_currentVirtualFrame = virtualFrameIndex;

    for (auto& chunk : m_chunks) {
        chunk.buffer.reclaim(chunk.watermarks[virtualFrameIndex]);
    }

    // Evict chunks that haven't been touched for kMaxVirtualFrames + grace frames.
    // The primary chunk at index 0 is preserved to avoid recreate thrash.
    if (m_chunks.size() > 1) {
        constexpr uint64_t kEvictionThreshold = kMaxVirtualFrames + kEvictionGraceFrames;
        const auto newEnd = std::remove_if(
            m_chunks.begin() + 1, m_chunks.end(), [&](const Chunk& c) {
                return (m_currentFrame - c.lastUsedFrame) > kEvictionThreshold;
            });
        if (newEnd != m_chunks.end()) {
            m_chunks.erase(newEnd, m_chunks.end());
            m_activeChunkIdx = 0;
        }
    }
}

void VulkanStagingBelt::endFrame() {
    for (auto& chunk : m_chunks) {
        chunk.watermarks[m_currentVirtualFrame] = chunk.buffer.getHead();
    }
}

StagingAllocation VulkanStagingBelt::stageData(const void* data, const VkDeviceSize size) {
    const auto tryAllocateAt = [&](const size_t idx) -> std::optional<StagingAllocation> {
        auto alloc = m_chunks[idx].buffer.allocate(size);
        if (!alloc.has_value()) {
            return std::nullopt;
        }
        std::memcpy(alloc->mappedPtr, data, size);
        m_chunks[idx].lastUsedFrame = m_currentFrame;
        m_activeChunkIdx = idx;
        return alloc;
    };

    if (m_activeChunkIdx < m_chunks.size()) {
        if (auto alloc = tryAllocateAt(m_activeChunkIdx); alloc.has_value()) {
            return *alloc;
        }
    }

    for (size_t i = 0; i < m_chunks.size(); ++i) {
        if (i == m_activeChunkIdx) {
            continue;
        }
        if (auto alloc = tryAllocateAt(i); alloc.has_value()) {
            return *alloc;
        }
    }

    // No chunk has space; create one sized to fit.
    const VkDeviceSize alignedSize = (size + m_alignment - 1) & ~(m_alignment - 1);
    const VkDeviceSize chunkSize = std::max(m_initialCapacity, alignedSize);
    addChunk(chunkSize);
    auto alloc = tryAllocateAt(m_chunks.size() - 1);
    CRISP_CHECK(
        alloc.has_value(),
        "freshly added staging chunk of {} bytes failed to satisfy a {}-byte allocation",
        chunkSize,
        size);
    return *alloc;
}

void VulkanStagingBelt::reclaimAll() {
    for (auto& chunk : m_chunks) {
        chunk.buffer.reclaimAll();
        chunk.watermarks.fill(0);
    }
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
    vkCmdCopyBuffer(cmdBuffer, alloc.buffer, dstBuffer.getHandle(), 1, &region);
}

void VulkanStagingBelt::uploadBufferBlocking(
    const VulkanQueue& queue,
    const VulkanBuffer& dstBuffer,
    const VkDeviceSize dstOffset,
    const void* data,
    const VkDeviceSize size) {
    if (size > m_initialCapacity) {
        spdlog::warn(
            "VulkanStagingBelt: upload of {} bytes exceeds initial chunk capacity of {}. Using temporary staging "
            "buffer.",
            size,
            m_initialCapacity);

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
        vkCmdCopyBuffer(cmdBuffer, alloc.buffer, dstBuffer.getHandle(), 1, &region);
    });

    // Queue is idle after submitAndWait; drain the chunk we staged into.
    auto& chunk = m_chunks[m_activeChunkIdx];
    chunk.buffer.reclaim(chunk.buffer.getHead());
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
        alloc.buffer,
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

} // namespace crisp
