#pragma once

#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanResource.hpp>

#include <Crisp/Coroutines/Task.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class VulkanDevice;
class VulkanImageView;
class VulkanFramebuffer;
class VulkanSwapChain;

struct RenderTargetInfo
{
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
};

struct RenderTarget
{
    RenderTargetInfo info;
    std::unique_ptr<VulkanImage> image;
};

struct AttachmentMapping
{
    uint32_t renderTargetIndex;
    VkImageSubresourceRange subresource;
    bool bufferOverDepthSlices{false};
};

struct RenderPassDescription
{
    VkExtent2D renderArea{0, 0};
    bool isSwapChainDependent{false};
    uint32_t subpassCount{0};
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<AttachmentMapping> attachmentMappings;
    bool bufferedRenderTargets{true};
    bool allocateRenderTargets{true};
    std::vector<RenderTarget*> renderTargets;
};

class VulkanRenderPass final : public VulkanResource<VkRenderPass, vkDestroyRenderPass>
{
public:
    // VulkanRenderPass(
    //     const VulkanDevice& device,
    //     std::vector<std::shared_ptr<VulkanImage>>&& renderTargets,
    //     bool isSwapChainDependent,
    //     uint32_t subpassCount);
    VulkanRenderPass(
        const VulkanDevice& device, VkRenderPass renderPassHandle, RenderPassDescription&& renderPassDescription);
    ~VulkanRenderPass();

    void recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent);

    VkExtent2D getRenderArea() const;
    VkViewport createViewport() const;
    VkRect2D createScissor() const;

    void begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex, VkSubpassContents content) const;
    void end(VkCommandBuffer cmdBuffer, uint32_t frameIndex);
    void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

    VulkanImage& getRenderTarget(uint32_t index) const;
    const VulkanImageView& getRenderTargetView(uint32_t attachmentIndex, uint32_t frameIndex) const;
    std::vector<VulkanImageView*> getRenderTargetViews(uint32_t renderTargetIndex) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(
        const VulkanDevice& device, uint32_t attachmentIndex, uint32_t frameCount) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(
        const VulkanDevice& device, uint32_t attachmentIndex, uint32_t baseLayer, uint32_t layerCount) const;

    inline uint32_t getNumSubpasses() const
    {
        return m_subpassCount;
    }

    inline VkSampleCountFlagBits getDefaultSampleCount() const
    {
        return m_defaultSampleCount;
    }

    const VulkanFramebuffer* getFramebuffer(uint32_t frameIdx) const
    {
        return m_framebuffers.at(frameIdx).get();
    }

    std::unique_ptr<VulkanFramebuffer>& getFramebuffer(uint32_t frameIdx)
    {
        return m_framebuffers.at(frameIdx);
    }

    /*inline const std::string& getName() const
    {
        return m_resouceDeallo;
    }*/

    void updateInitialLayouts(VkCommandBuffer cmdBuffer);

protected:
    // void createRenderTargets(const VulkanDevice& device);
    void createRenderTargetViewsAndFramebuffers(const VulkanDevice& device);

    VkExtent3D getRenderAreaExtent() const;

    void createResources(const VulkanDevice& device);
    void freeResources();

    /*void setDepthRenderTargetInfo(
        uint32_t index, VkImageUsageFlags additionalFlags, VkClearDepthStencilValue clearValue = {0.0f, 0});
    void setColorRenderTargetInfo(
        uint32_t index, VkImageUsageFlags additionalFlags, VkClearColorValue clearValue = {0.0f, 0.0f, 0.0f, 0.0f});*/

    // High-level metadata.
    bool m_isSwapChainDependent;
    bool m_allocateResources{true};
    VkExtent2D m_renderArea;
    uint32_t m_subpassCount;

    VkSampleCountFlagBits m_defaultSampleCount;

    std::vector<RenderTarget*> m_renderTargets;

    std::vector<VkAttachmentDescription> m_attachmentDescriptions;
    std::vector<AttachmentMapping> m_attachmentMappings;
    std::vector<VkClearValue> m_attachmentClearValues;

    // Accessed view is indexed as [frameIdx][attachmentViewIdx].
    std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_renderTargetViews;

    std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;

    // std::string m_name;

    // bool m_bufferedRenderTargets{true};
};
} // namespace crisp
