#pragma once

#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

#include <memory>
#include <vector>

namespace crisp {
struct RenderTargetInfo {
    // Required to set correct initial layout.
    VkPipelineStageFlags initSrcStageFlags{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags initDstStageFlags{};

    // Used by attachments to clear themselves.
    VkClearValue clearValue{};

    VkFormat format{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
    uint32_t layerCount{1};
    uint32_t mipmapCount{1};
    uint32_t depthSlices{1};

    VkImageCreateFlags createFlags{};
    VkImageUsageFlags usage{};

    // Indicates if the render target should be buffered over virtual frames.
    bool buffered{false};

    // Indicates if the render target has a fixed size. If not set, it is resized to the swap chain.
    VkExtent2D size{0, 0};
    bool isSwapChainDependent{false};
};

struct RenderTarget {
    RenderTargetInfo info;
    std::unique_ptr<VulkanImage> image;
};

struct AttachmentMapping {
    uint32_t renderTargetIndex{};
    VkImageSubresourceRange subresource{};
    bool bufferOverDepthSlices{false};
};

struct RenderPassParameters {
    uint32_t subpassCount{0};
    VkExtent2D renderArea{0, 0};

    // If true, indicates that this render pass should resize its views according to the swap chain extent.
    // Otherwise, the render pass has a fixed size.
    bool isSwapChainDependent{false};

    // If true, indicates that the render pass should allocate its own image views.
    // Otherwise, attachment views are supplied from the outer context.
    bool allocateAttachmentViews{true};

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<AttachmentMapping> attachmentMappings;

    std::vector<RenderTargetInfo> renderTargetInfos;
    std::vector<VulkanImage*> renderTargets;
};

class VulkanRenderPass final : public VulkanResource<VkRenderPass> {
public:
    VulkanRenderPass(const VulkanDevice& device, VkRenderPass handle, RenderPassParameters&& parameters);
    VulkanRenderPass(
        const VulkanDevice& device,
        VkRenderPass handle,
        RenderPassParameters&& parameters,
        std::vector<VkClearValue>&& clearValues);

    void recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent);

    VkExtent2D getRenderArea() const;
    VkViewport createViewport() const;
    VkRect2D createScissor() const;

    void begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex, VkSubpassContents content) const;
    void end(VkCommandBuffer cmdBuffer, uint32_t frameIndex);
    void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

    VulkanImage& getRenderTarget(uint32_t index) const;
    VulkanImage& getRenderTargetFromAttachmentIndex(uint32_t attachmentIndex) const;
    const VulkanImageView& getAttachmentView(uint32_t attachmentIndex, uint32_t frameIndex) const;
    std::vector<VulkanImageView*> getAttachmentViews(uint32_t renderTargetIndex) const;

    inline uint32_t getSubpassCount() const {
        return m_params.subpassCount;
    }

    // Used by default swap chain to update its framebuffers.
    std::unique_ptr<VulkanFramebuffer>& getFramebuffer(uint32_t frameIdx) {
        return m_framebuffers.at(frameIdx);
    }

    void updateInitialLayouts(VkCommandBuffer cmdBuffer);

protected:
    void createRenderTargetViewsAndFramebuffers(const VulkanDevice& device);
    void createResources(const VulkanDevice& device);
    void freeResources();

    RenderPassParameters m_params;
    std::vector<VkClearValue> m_attachmentClearValues;

    // Accessed view is indexed as [frameIdx][attachmentViewIdx].
    std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_attachmentViews;
    std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;
};
} // namespace crisp
