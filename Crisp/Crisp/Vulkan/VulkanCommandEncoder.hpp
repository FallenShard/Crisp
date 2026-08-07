#pragma once

#include <span>

#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {

class VulkanCommandEncoder {
public:
    explicit VulkanCommandEncoder(VkCommandBuffer cmdBuffer);

    void setViewport(const VkViewport& viewport) const;
    void setScissor(const VkRect2D& scissorRect) const;
    void bindPipeline(const VulkanPipeline& pipeline) const;

    void insertBarrier(const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(VkBuffer buffer, const VulkanSynchronizationScope& scope) const {
        insertBufferMemoryBarrier(buffer, 0, VK_WHOLE_SIZE, scope);
    }
    void insertBufferMemoryBarrier(
        const VkDescriptorBufferInfo& bufferInfo, const VulkanSynchronizationScope& scope) const {
        insertBufferMemoryBarrier(bufferInfo.buffer, bufferInfo.offset, bufferInfo.range, scope);
    }
    void insertBufferMemoryBarrier(const VulkanBuffer& buffer, const VulkanSynchronizationScope& scope) const {
        insertBufferMemoryBarrier(buffer.getHandle(), scope);
    }
    void insertBufferMemoryBarriers(std::span<const VkBufferMemoryBarrier2> barriers) const;
    void insertImageMemoryBarrier(const VkImageMemoryBarrier2& barrier) const;
    void transferBufferOwnership(
        VkBuffer buffer,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex,
        const VulkanSynchronizationScope& scope) const;

    void copyBuffer(VkBuffer src, VkBuffer dst, std::span<const VkBufferCopy> regions) const;
    void copyBuffer(const VulkanBuffer& src, const VulkanBuffer& dst, const VkBufferCopy& region) const;
    void copyBuffer(const VulkanBuffer& src, const VulkanBuffer& dst) const;

    void transitionLayout(VulkanImage& image, VkImageLayout newLayout, const VulkanSynchronizationScope& scope) const;
    void transitionLayout(
        VulkanImage& image,
        VkImageLayout newLayout,
        const VulkanSynchronizationScope& scope,
        const VkImageSubresourceRange& range) const;
    void transitionLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        const VulkanSynchronizationScope& scope,
        const VkImageSubresourceRange& range) const;

    void beginRendering(const VkRenderingInfo& renderingInfo) const;
    void endRendering() const;

    void copyBufferToImage(
        VkBuffer src, VulkanImage& dst, std::span<const VkBufferImageCopy> regions) const;
    void copyBufferToImage(
        const VulkanBuffer& src, VulkanImage& dst, const VkBufferImageCopy& region) const;
    void copyImageToBuffer(
        const VulkanImage& src, VkBuffer dst, std::span<const VkBufferImageCopy> regions) const;
    void copyImageToBuffer(
        const VulkanImage& src, const VulkanBuffer& dst, const VkBufferImageCopy& region) const;
    void blitImage(
        const VulkanImage& src,
        VulkanImage& dst,
        const VkImageBlit& region,
        VkFilter filter = VK_FILTER_LINEAR) const;
    void generateMipmaps(VulkanImage& image, const VulkanSynchronizationStage& initialStage = kTransferRead) const;

    void dispatchCompute(const VkExtent3D& workGroupCount) const;
    void drawMeshTasks(const VkExtent3D& groupCount) const;
    void drawMeshTasks(uint32_t groupCount) const;
    void traceRays(std::span<const VkStridedDeviceAddressRegionKHR> bindingRegions, const VkExtent2D& gridSize) const;

    VkCommandBuffer getHandle() const {
        return m_cmdBuffer;
    }

private:
    VkCommandBuffer m_cmdBuffer;
};

} // namespace crisp
