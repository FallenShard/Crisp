#pragma once

#include <array>
#include <cstddef>
#include <span>

#include <Crisp/Vulkan/Rhi/VulkanAccelerationStructure.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDescriptorSetBinding.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipelineStatsQueryPool.hpp>
#include <Crisp/Vulkan/Rhi/VulkanQueue.hpp>
#include <Crisp/Vulkan/Rhi/VulkanTimestampQueryPool.hpp>
#include <Crisp/Vulkan/VulkanDescriptorHeap.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {

class VulkanCommandEncoder {
public:
    explicit VulkanCommandEncoder(VkCommandBuffer cmdBuffer);

    void setViewport(const VkViewport& viewport) const;
    void setScissor(const VkRect2D& scissorRect) const;

    void setLineWidth(float lineWidth) const;
    void setDepthBias(float constantFactor, float slopeFactor, float clamp = 0.0f) const;
    void setBlendConstants(const std::array<float, 4>& blendConstants) const;
    void setDepthBounds(float minDepthBounds, float maxDepthBounds) const;
    void setStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask) const;
    void setStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask) const;
    void setStencilReference(VkStencilFaceFlags faceMask, uint32_t reference) const;
    void setCullMode(VkCullModeFlags cullMode) const;
    void setFrontFace(VkFrontFace frontFace) const;
    void setPrimitiveTopology(VkPrimitiveTopology topology) const;
    void setDepthTestEnable(bool enabled) const;
    void setDepthWriteEnable(bool enabled) const;
    void setDepthCompareOp(VkCompareOp compareOp) const;
    void setDepthBoundsTestEnable(bool enabled) const;
    void setStencilTestEnable(bool enabled) const;
    void setStencilOp(
        VkStencilFaceFlags faceMask,
        VkStencilOp failOp,
        VkStencilOp passOp,
        VkStencilOp depthFailOp,
        VkCompareOp compareOp) const;
    void setRasterizerDiscardEnable(bool enabled) const;
    void setDepthBiasEnable(bool enabled) const;
    void setPrimitiveRestartEnable(bool enabled) const;
    void bindPipeline(const VulkanPipeline& pipeline) const;
    void bindDescriptorSets(const VulkanDescriptorSetBinding& binding) const;
    void bindDescriptorSets(
        VkPipelineBindPoint bindPoint,
        VkPipelineLayout layout,
        uint32_t firstSet,
        std::span<const VkDescriptorSet> sets,
        std::span<const uint32_t> dynamicOffsets = {}) const;
    void bindResourceHeap(const VulkanResourceHeap& heap) const;
    void bindSamplerHeap(const VulkanSamplerHeap& heap) const;
    void bindVertexBuffers(
        uint32_t firstBinding, std::span<const VkBuffer> buffers, std::span<const VkDeviceSize> offsets) const;
    void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const;
    void draw(
        uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) const;
    void drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount = 1,
        uint32_t firstIndex = 0,
        int32_t vertexOffset = 0,
        uint32_t firstInstance = 0) const;

    void insertBarrier(const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(
        VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(VkBuffer buffer, const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(
        const VkDescriptorBufferInfo& bufferInfo, const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(const VulkanBuffer& buffer, const VulkanSynchronizationScope& scope) const;
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

    void copyBufferToImage(VkBuffer src, VulkanImage& dst, std::span<const VkBufferImageCopy> regions) const;
    void copyBufferToImage(const VulkanBuffer& src, VulkanImage& dst, const VkBufferImageCopy& region) const;
    void copyBufferToImage(const VulkanBuffer& src, VulkanImage& dst) const;
    void copyImageToBuffer(const VulkanImage& src, VkBuffer dst, std::span<const VkBufferImageCopy> regions) const;
    void copyImageToBuffer(const VulkanImage& src, const VulkanBuffer& dst, const VkBufferImageCopy& region) const;
    void blitImage(
        const VulkanImage& src, VulkanImage& dst, const VkImageBlit& region, VkFilter filter = VK_FILTER_LINEAR) const;
    void generateMipmaps(VulkanImage& image, const VulkanSynchronizationStage& initialStage = kTransferRead) const;

    void dispatchCompute(const VkExtent3D& workGroupCount) const;
    void drawMeshTasks(const VkExtent3D& groupCount) const;
    void drawMeshTasks(uint32_t groupCount) const;
    void drawMeshTasksIndirect(
        const VulkanBuffer& argsBuffer,
        VkDeviceSize offset,
        uint32_t drawCount,
        uint32_t stride = sizeof(VkDrawMeshTasksIndirectCommandEXT)) const;
    void drawMeshTasksIndirectCount(
        const VulkanBuffer& argsBuffer,
        VkDeviceSize offset,
        const VulkanBuffer& countBuffer,
        VkDeviceSize countOffset,
        uint32_t maxDrawCount,
        uint32_t stride = sizeof(VkDrawMeshTasksIndirectCommandEXT)) const;
    void traceRays(std::span<const VkStridedDeviceAddressRegionKHR> bindingRegions, const VkExtent2D& gridSize) const;

    void updateBuffer(const VulkanBuffer& buffer, std::span<const std::byte> data) const;
    void buildAccelerationStructure(VulkanAccelerationStructure& accelerationStructure) const;

    void writeTimestamp(
        const VulkanTimestampQueryPool& queryPool, VkPipelineStageFlags2 stage, uint32_t queryIndex) const;

    void beginQuery(const VulkanPipelineStatsQueryPool& queryPool, uint32_t queryIndex = 0) const;
    void endQuery(const VulkanPipelineStatsQueryPool& queryPool, uint32_t queryIndex = 0) const;

    template <typename T, typename... Ts>
    void setPushConstants(
        const VulkanPipelineLayout& layout, VkShaderStageFlags stageFlags, const T& value, const Ts&... rest) const {
        setPushConstantsAt(layout, stageFlags, 0, value, rest...);
    }

    void setPushConstants(const VulkanPipelineLayout& layout, std::span<const std::byte> data) const;
    void pushData(std::span<const std::byte> data) const;

    VkCommandBuffer getHandle() const {
        return m_cmdBuffer;
    }

private:
    template <typename T, typename... Ts>
    void setPushConstantsAt(
        const VulkanPipelineLayout& layout,
        VkShaderStageFlags stageFlags,
        uint32_t offset,
        const T& value,
        const Ts&... rest) const {
        vkCmdPushConstants(m_cmdBuffer, layout.getHandle(), stageFlags, offset, sizeof(T), &value);
        if constexpr (sizeof...(Ts) > 0) {
            setPushConstantsAt(layout, stageFlags, offset + sizeof(T), rest...);
        }
    }

    VkCommandBuffer m_cmdBuffer;
};

template <typename Func>
void submitAndWait(const VulkanQueue& queue, Func&& func) {
    queue.submitAndWait([&func](const VkCommandBuffer cmdBuffer) { func(VulkanCommandEncoder(cmdBuffer)); });
}

} // namespace crisp
