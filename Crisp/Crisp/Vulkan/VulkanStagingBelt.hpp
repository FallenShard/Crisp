#pragma once

#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

#include <array>
#include <span>
#include <type_traits>
#include <vector>

namespace crisp {

class VulkanImage;
class VulkanQueue;

class VulkanStagingBelt {
public:
    VulkanStagingBelt(VulkanDevice& device, VkDeviceSize capacity);

    // --- Per-frame streaming ---
    void beginFrame(uint32_t virtualFrameIndex);
    void endFrame();

    // --- Low-level: stage data and get allocation info ---
    // Copies `data` into the ring buffer and returns the allocation.
    // Use this when you need to record custom copy commands (e.g. with layout transitions).
    // Call reclaimAll() after blocking submits.
    StagingAllocation stageData(const void* data, VkDeviceSize size);
    VkBuffer getStagingBufferHandle() const;
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

    VulkanStagingBuffer& getStagingBuffer();
    const VulkanStagingBuffer& getStagingBuffer() const;

private:
    VulkanDevice* m_device;
    VulkanStagingBuffer m_stagingBuffer;

    static constexpr uint32_t kMaxVirtualFrames = 4;
    std::array<VkDeviceSize, kMaxVirtualFrames> m_frameWatermarks{};
    uint32_t m_currentVirtualFrame{0};
};

} // namespace crisp
