#pragma once

#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

#include <array>
#include <span>
#include <type_traits>
#include <vector>

namespace crisp {

class VulkanImage;
class VulkanQueue;

struct ReadbackBuffer {
    std::unique_ptr<VulkanBuffer> buffer;

    template <typename T>
    const T* getMappedData() const {
        return buffer->getHostVisibleData<T>();
    }

    VkDeviceSize getSize() const {
        return buffer ? buffer->getSize() : 0;
    }

    explicit operator bool() const {
        return static_cast<bool>(buffer);
    }
};

class VulkanStagingBelt {
public:
    VulkanStagingBelt(VulkanDevice& device, VkDeviceSize initialCapacity);

    // --- Per-frame streaming ---
    void beginFrame(uint32_t virtualFrameIndex);
    void endFrame();

    // --- Low-level: stage data and get allocation info ---
    // Copies `data` into a chunk and returns the allocation (carrying its source VkBuffer).
    // Use this when you need to record custom copy commands (e.g. with layout transitions).
    // Call reclaimAll() after blocking submits.
    StagingAllocation stageData(const void* data, VkDeviceSize size);
    void reclaimAll();

    // --- Buffer uploads ---
    void uploadBuffer(
        VkCommandBuffer cmdBuffer,
        const VulkanBuffer& dstBuffer,
        VkDeviceSize dstOffset,
        const void* data,
        VkDeviceSize size);

    void uploadBufferBlocking(
        const VulkanQueue& queue,
        const VulkanBuffer& dstBuffer,
        VkDeviceSize dstOffset,
        const void* data,
        VkDeviceSize size);

    void uploadBufferBlocking(
        const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const void* data, VkDeviceSize size);

    // --- Buffer / image downloads (GPU -> host readback) ---
    // Allocates a host-visible buffer, records a copy from `src` into it, and returns the buffer.
    // The mapped data is only safe to read after the submission containing `cmd` has completed
    // (caller's responsibility — typically via fence wait, e.g. waiting Renderer::NumVirtualFrames).
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

    // --- Typed overloads: single struct ---
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void uploadBuffer(
        VkCommandBuffer cmdBuffer, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, const T& value) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, &value, sizeof(T));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void uploadBufferBlocking(const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const T& value) {
        uploadBufferBlocking(queue, dstBuffer, &value, sizeof(T));
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void uploadBufferBlocking(
        const VulkanQueue& queue, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, const T& value) {
        uploadBufferBlocking(queue, dstBuffer, dstOffset, &value, sizeof(T));
    }

    // --- Typed overloads: std::vector ---
    template <typename T>
    void uploadBuffer(
        VkCommandBuffer cmdBuffer,
        const VulkanBuffer& dstBuffer,
        VkDeviceSize dstOffset,
        const std::vector<T>& data) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, data.data(), data.size() * sizeof(T));
    }

    template <typename T>
    void uploadBufferBlocking(
        const VulkanQueue& queue, const VulkanBuffer& dstBuffer, const std::vector<T>& data) {
        uploadBufferBlocking(queue, dstBuffer, data.data(), data.size() * sizeof(T));
    }

    template <typename T>
    void uploadBufferBlocking(
        const VulkanQueue& queue,
        const VulkanBuffer& dstBuffer,
        VkDeviceSize dstOffset,
        const std::vector<T>& data) {
        uploadBufferBlocking(queue, dstBuffer, dstOffset, data.data(), data.size() * sizeof(T));
    }

    // --- Typed overloads: std::span ---
    template <typename T>
    void uploadBuffer(
        VkCommandBuffer cmdBuffer, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, std::span<T> data) {
        uploadBuffer(cmdBuffer, dstBuffer, dstOffset, data.data(), data.size_bytes());
    }

    template <typename T>
    void uploadBufferBlocking(const VulkanQueue& queue, const VulkanBuffer& dstBuffer, std::span<T> data) {
        uploadBufferBlocking(queue, dstBuffer, data.data(), data.size_bytes());
    }

    template <typename T>
    void uploadBufferBlocking(
        const VulkanQueue& queue, const VulkanBuffer& dstBuffer, VkDeviceSize dstOffset, std::span<T> data) {
        uploadBufferBlocking(queue, dstBuffer, dstOffset, data.data(), data.size_bytes());
    }

private:
    static constexpr uint32_t kMaxVirtualFrames = 4;
    static constexpr uint64_t kEvictionGraceFrames = 60;

    struct Chunk {
        VulkanStagingBuffer buffer;
        std::array<VkDeviceSize, kMaxVirtualFrames> watermarks{};
        uint64_t lastUsedFrame{0};
    };

    Chunk& addChunk(VkDeviceSize capacity);

    VulkanDevice* m_device;
    VkDeviceSize m_initialCapacity;
    VkDeviceSize m_alignment;

    std::vector<Chunk> m_chunks;
    size_t m_activeChunkIdx{0};

    uint64_t m_currentFrame{0};
    uint32_t m_currentVirtualFrame{0};
};

} // namespace crisp
