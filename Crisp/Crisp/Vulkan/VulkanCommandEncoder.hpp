#pragma once

#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {

class VulkanCommandEncoder {
public:
    explicit VulkanCommandEncoder(VkCommandBuffer cmdBuffer);

    void setViewport(const VkViewport& viewport) const;
    void setScissor(const VkRect2D& scissorRect) const;
    void bindPipeline(const VulkanPipeline& pipeline) const;

    void insertBarrier(const VulkanSynchronizationScope& scope) const;
    void insertBufferMemoryBarrier(const VulkanBuffer& buffer, const VulkanSynchronizationScope& scope) const;
    void transitionLayout(VulkanImage& image, VkImageLayout newLayout, const VulkanSynchronizationScope& scope) const;
    void transitionLayout(
        VulkanImage& image,
        VkImageLayout newLayout,
        const VulkanSynchronizationScope& scope,
        const VkImageSubresourceRange& range) const;

    void beginRenderPass(const VulkanRenderPass& renderPass, const VulkanFramebuffer& framebuffer) const;
    void endRenderPass(VulkanRenderPass& renderPass) const;

    VkCommandBuffer getHandle() const {
        return m_cmdBuffer;
    }

private:
    VkCommandBuffer m_cmdBuffer;
};

} // namespace crisp