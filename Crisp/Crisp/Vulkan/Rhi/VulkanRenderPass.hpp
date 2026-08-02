#pragma once

#include <memory>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>

namespace crisp {
struct RenderTargetInfo {
    // Required to set correct initial layout.
    VkPipelineStageFlags2 initSrcStageFlags{VK_PIPELINE_STAGE_2_NONE};
    VkPipelineStageFlags2 initDstStageFlags{VK_PIPELINE_STAGE_2_NONE};

    // Used by attachments to clear themselves.
    VkClearValue clearValue{};

    VkFormat format{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
    uint32_t layerCount{1};
    uint32_t mipmapCount{1};
    uint32_t depthSlices{1};

    VkImageCreateFlags createFlags{};
    VkImageUsageFlags usage{};

    // Indicates if the render target has a fixed size. If not set, it is resized to the swap chain.
    VkExtent2D size{0, 0};
    bool isSwapChainDependent{false};
};

struct RenderTarget {
    RenderTargetInfo info;
    std::unique_ptr<VulkanImage> image;
};

struct RenderPassParameters {
    uint32_t subpassCount{0};
    VkExtent2D renderArea{0, 0};

    std::vector<VkClearValue> clearValues;

    std::vector<VkAttachmentDescription> attachmentDescriptions;
};

class VulkanRenderPass final : public VulkanResource<VkRenderPass> {
public:
    VulkanRenderPass(const VulkanDevice& device, VkRenderPass handle, RenderPassParameters&& parameters);

    void setRenderArea(const VkExtent2D& extent);
    VkExtent2D getRenderArea() const;
    VkViewport createViewport() const;
    VkRect2D createScissor() const;

    void begin(VkCommandBuffer cmdBuffer, const VulkanFramebuffer& framebuffer, VkSubpassContents content) const;
    void end(VkCommandBuffer cmdBuffer);
    void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

    VulkanImage& getRenderTarget(uint32_t attachmentIndex) const;
    const VulkanImageView& getAttachmentView(uint32_t attachmentIndex) const;

    uint32_t getSubpassCount() const {
        return m_params.subpassCount;
    }

    const std::vector<VkClearValue>& getClearValues() const {
        return m_params.clearValues;
    }

    VkImageLayout getFinalLayout(const uint32_t attachmentIndex) const {
        return m_params.attachmentDescriptions.at(attachmentIndex).finalLayout;
    }

protected:
    RenderPassParameters m_params;

    std::vector<std::unique_ptr<VulkanImageView>> m_attachmentViews;
};
} // namespace crisp
