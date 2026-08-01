#include <Crisp/Vulkan/VulkanStagingBelt.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

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
        .lastUsedTick = m_tick,
    });
}

void VulkanStagingBelt::setRetirementValue(const uint64_t value) {
    m_retirementValue = value;
}

uint64_t VulkanStagingBelt::getRetirementValue() const {
    return m_retirementValue;
}

void VulkanStagingBelt::collect(const uint64_t completedValue) {
    ++m_tick;

    for (auto& chunk : m_chunks) {
        size_t retiredCount = 0;
        while (retiredCount < chunk.pending.size() && chunk.pending[retiredCount].retirementValue <= completedValue) {
            ++retiredCount;
        }
        if (retiredCount == 0) {
            continue;
        }

        chunk.buffer.reclaim(chunk.pending[retiredCount - 1].head);
        chunk.pending.erase(chunk.pending.begin(), chunk.pending.begin() + retiredCount);
    }

    evictStaleChunks();
}

void VulkanStagingBelt::evictStaleChunks() {
    // The primary chunk at index 0 is preserved to avoid recreate thrash.
    if (m_chunks.size() <= 1) {
        return;
    }

    const auto newEnd = std::remove_if(m_chunks.begin() + 1, m_chunks.end(), [this](const Chunk& c) {
        return c.pending.empty() && (m_tick - c.lastUsedTick) > kEvictionGraceTicks;
    });
    if (newEnd != m_chunks.end()) {
        m_chunks.erase(newEnd, m_chunks.end());
        m_activeChunkIdx = 0;
    }
}

StagingAllocation VulkanStagingBelt::stageData(const void* data, const VkDeviceSize size) {
    return stageData(data, size, m_retirementValue);
}

StagingAllocation VulkanStagingBelt::stageData(
    const void* data, const VkDeviceSize size, const uint64_t retirementValue) {
    const auto tryAllocateAt = [&](const size_t idx) -> std::optional<StagingAllocation> {
        auto& chunk = m_chunks[idx];
        auto alloc = chunk.buffer.allocate(size);
        if (!alloc.has_value()) {
            return std::nullopt;
        }
        std::memcpy(alloc->mappedPtr, data, size);

        if (!chunk.pending.empty() && chunk.pending.back().retirementValue == retirementValue) {
            chunk.pending.back().head = chunk.buffer.getHead();
        } else {
            chunk.pending.push_back({retirementValue, chunk.buffer.getHead()});
        }
        chunk.lastUsedTick = m_tick;
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
