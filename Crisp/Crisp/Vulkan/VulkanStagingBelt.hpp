#pragma once

#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace crisp {

class VulkanImage;

struct ReadbackBuffer {
    std::unique_ptr<VulkanBuffer> buffer;

    template <typename T>
    const T* getMappedData() const {
        return buffer->getHostVisibleData<T>();
    }

    VkDeviceSize getSize() const {
        return buffer ? buffer->getSize() : 0;
    }
};

// A GPU -> host copy in flight. The recorded copy is only readable once the submission carrying it has signaled,
// so the owner keeps this around and polls it with FrameContext::completedValue until it yields data.
class AsyncReadback {
public:
    void record(ReadbackBuffer&& buffer, const uint64_t completionValue) {
        m_buffer = std::move(buffer);
        m_completionValue = completionValue;
    }

    bool isPending() const {
        return static_cast<bool>(m_buffer.buffer);
    }

    // Returns the mapped data once the GPU has finished writing it, nullopt while the copy is still in flight.
    template <typename T>
    std::optional<std::span<const T>> tryRead(const uint64_t completedValue) {
        if (!m_buffer.buffer || completedValue < m_completionValue) {
            return std::nullopt;
        }
        m_buffer.buffer->invalidateMappedRange();
        return std::span<const T>(m_buffer.getMappedData<T>(), m_buffer.getSize() / sizeof(T));
    }

    void reset() {
        m_buffer = {};
        m_completionValue = 0;
    }

private:
    ReadbackBuffer m_buffer;
    uint64_t m_completionValue{0};
};

class VulkanStagingBelt {
public:
    VulkanStagingBelt(VulkanDevice& device, VkDeviceSize initialCapacity);

    // --- Ring reclamation ---
    // Data staged from now on is held until this value is reached, mirroring VulkanResourceDeallocator.
    void setRetirementValue(uint64_t value);
    uint64_t getRetirementValue() const;

    // Reclaims the ring space of every staged range the owner's clock has passed.
    void collect(uint64_t completedValue);

    // --- Low-level: stage data and get allocation info ---
    // Copies `data` into a chunk and returns the allocation (carrying its source VkBuffer).
    // Use this when you need to record custom copy commands (e.g. with layout transitions).
    StagingAllocation stageData(const void* data, VkDeviceSize size);

    // --- Buffer uploads ---
    void uploadBuffer(
        VkCommandBuffer cmdBuffer,
        const VulkanBuffer& dstBuffer,
        VkDeviceSize dstOffset,
        const void* data,
        VkDeviceSize size);

    // --- Buffer / image downloads (GPU -> host readback) ---
    // Allocates a host-visible buffer, records a copy from `src` into it, and returns the buffer.
    // The mapped data is only safe to read after the submission containing `cmd` has completed
    // (caller's responsibility — typically by waiting on the timeline value that submission signals).
    // The source must already be in a layout/access that supports TRANSFER_READ.
    ReadbackBuffer downloadBuffer(
        VkCommandBuffer cmd, const VulkanBuffer& src, VkDeviceSize srcOffset, VkDeviceSize size);

    ReadbackBuffer downloadImage(
        VkCommandBuffer cmd,
        const VulkanImage& src,
        VkExtent3D extent,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t mipLevel,
        VkDeviceSize size);

    // --- Image uploads ---
    // Records a vkCmdCopyBufferToImage. The image must already be in TRANSFER_DST_OPTIMAL layout.
    void uploadImage(
        VkCommandBuffer cmdBuffer,
        const VulkanImage& dstImage,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t mipLevel,
        const void* data,
        VkDeviceSize size);

    void uploadImage(
        VkCommandBuffer cmdBuffer,
        const VulkanImage& dstImage,
        VkExtent3D extent,
        uint32_t baseLayer,
        uint32_t numLayers,
        uint32_t mipLevel,
        const void* data,
        VkDeviceSize size);

    // --- Typed overloads ---
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void uploadBuffer(VkCommandBuffer cmdBuffer, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, const T& value) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, &value, sizeof(T));
    }

    template <typename T>
    void uploadBuffer(
        VkCommandBuffer cmdBuffer, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, const std::vector<T>& data) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, data.data(), data.size() * sizeof(T));
    }

    template <typename T>
    void uploadBuffer(
        VkCommandBuffer cmdBuffer, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, std::span<T> data) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, data.data(), data.size_bytes());
    }

private:
    static constexpr uint64_t kEvictionGraceTicks = 60;

    // A run of allocations sharing one retirement value. The ring only frees in allocation order, so these are
    // popped from the front and the chunk's tail follows the head of the last retired one.
    struct PendingRange {
        uint64_t retirementValue;
        VkDeviceSize head;
    };

    struct Chunk {
        VulkanStagingBuffer buffer;
        std::vector<PendingRange> pending;
        uint64_t lastUsedTick{0};
    };

    Chunk& addChunk(VkDeviceSize capacity);
    StagingAllocation stageData(const void* data, VkDeviceSize size, uint64_t retirementValue);
    void evictStaleChunks();

    VulkanDevice* m_device;
    VkDeviceSize m_initialCapacity;
    VkDeviceSize m_alignment;

    std::vector<Chunk> m_chunks;
    size_t m_activeChunkIdx{0};

    uint64_t m_retirementValue{0};

    // Counts collect() calls. Drives chunk eviction only, which is a staleness heuristic and not a safety property.
    uint64_t m_tick{0};
};

} // namespace crisp
