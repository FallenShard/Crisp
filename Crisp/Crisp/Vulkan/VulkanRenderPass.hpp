#pragma once

#include <memory>
#include <vector>

#include <CrispCore/Coroutines/Task.hpp>

#include "VulkanResource.hpp"

namespace crisp
{
class VulkanDevice;
class VulkanImage;
class VulkanImageView;
class VulkanFramebuffer;
class Renderer;

struct RenderTargetInfo
{
    VkImageUsageFlags usage;
    VkPipelineStageFlags initSrcStageFlags{ VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
    VkPipelineStageFlags initDstStageFlags;
    VkClearValue clearValue;

    inline void configureColorRenderTarget(VkImageUsageFlags additionalFlags,
        VkClearColorValue clearVal = { 0.0f, 0.0f, 0.0f, 0.0f })
    {
        initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalFlags;
        clearValue.color = clearVal;
    }

    inline void configureDepthRenderTarget(VkImageUsageFlags additionalFlags = 0,
        VkClearDepthStencilValue clearVal = { 0.0f, 0 })
    {
        initDstStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalFlags;
        clearValue.depthStencil = clearVal;
    }
};

struct RenderPassDescription
{
    std::optional<VkExtent2D> renderArea;
    bool isSwapChainDependent;
    uint32_t subpassCount;
    std::vector<VkAttachmentDescription> attachmentDescriptions;
    std::vector<RenderTargetInfo> renderTargetInfos;
    bool bufferedRenderTargets{ true };
};

class VulkanRenderPass : public VulkanResource<VkRenderPass, vkDestroyRenderPass>
{
public:
    VulkanRenderPass(Renderer& renderer, bool isWindowSizeDependent, uint32_t numSubpasses);
    VulkanRenderPass(Renderer& renderer, VkRenderPass renderPass, RenderPassDescription&& renderPassDescription);
    virtual ~VulkanRenderPass();

    void recreate(Renderer& renderer);

    VkExtent2D getRenderArea() const;
    VkViewport createViewport() const;
    VkRect2D createScissor() const;

    void begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex, VkSubpassContents content) const;
    void end(VkCommandBuffer cmdBuffer, uint32_t frameIndex);
    void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

    VulkanImage& getRenderTarget(uint32_t index) const;
    const VulkanImageView& getRenderTargetView(uint32_t renderTargetIndex, uint32_t frameIndex) const;
    std::vector<VulkanImageView*> getRenderTargetViews(uint32_t renderTargetIndex) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(const VulkanDevice& device, uint32_t index,
        uint32_t numFrames) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(const VulkanDevice& device, uint32_t index,
        uint32_t baseLayer, uint32_t numLayers) const;

    std::unique_ptr<VulkanImage> extractRenderTarget(uint32_t index);

    inline uint32_t getNumSubpasses() const
    {
        return m_numSubpasses;
    }

    VkSampleCountFlagBits getDefaultSampleCount() const;

    const VulkanFramebuffer* getFramebuffer(uint32_t frameIdx) const
    {
        return m_framebuffers.at(frameIdx).get();
    }

    inline const std::string& getName() const
    {
        return m_name;
    }
    inline void setName(std::string name)
    {
        m_name = std::move(name);
    }

    coro::Task<int> updateInitialLayouts(Renderer* renderer);

protected:
    void createRenderTargets(Renderer& renderer);
    void createRenderTargetViewsAndFramebuffers(const VulkanDevice& device, uint32_t layerCount);

    VkExtent3D getRenderAreaExtent() const;

    virtual void createResources(Renderer& renderer);
    void freeResources();

    void setDepthRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
        VkClearDepthStencilValue clearValue = { 0.0f, 0 });
    void setColorRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
        VkClearColorValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f });

    uint32_t m_numSubpasses;
    bool m_isWindowSizeDependent;
    VkExtent2D m_renderArea;
    VkSampleCountFlagBits m_defaultSampleCount;

    std::vector<VkClearValue> m_attachmentClearValues;
    std::vector<RenderTargetInfo> m_renderTargetInfos;
    std::vector<VkAttachmentDescription> m_attachmentDescriptions;
    std::vector<std::unique_ptr<VulkanImage>> m_renderTargets;
    std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_renderTargetViews; // numTargets x numFrames

    std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;

    std::string m_name;

    bool m_combinedImages{ false };
    bool m_bufferedRenderTargets{ true };
};
} // namespace crisp
