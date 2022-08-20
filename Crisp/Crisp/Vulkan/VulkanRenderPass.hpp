#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

#include <Crisp/Coroutines/Task.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class VulkanDevice;
class VulkanImage;
class VulkanImageView;
class VulkanFramebuffer;
class VulkanSwapChain;

struct RenderTargetInfo
{
    VkPipelineStageFlags initSrcStageFlags{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    VkPipelineStageFlags initDstStageFlags;
    VkClearValue clearValue;

    VkImageUsageFlags usage;
    uint32_t layerCount{1};
    uint32_t mipmapCount{1};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkSampleCountFlagBits sampleCount{};
    uint32_t depthSlices{1};
    VkImageCreateFlags createFlags{};
    std::optional<bool> buffered;
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
    std::vector<RenderTargetInfo> renderTargetInfos;
    bool bufferedRenderTargets{true};
    bool allocateRenderTargets{true};
};

class VulkanRenderPass final : public VulkanResource<VkRenderPass, vkDestroyRenderPass>
{
public:
    VulkanRenderPass(const VulkanDevice& device, bool isSwapChainDependent, uint32_t subpassCount);
    VulkanRenderPass(
        const VulkanDevice& device, VkRenderPass renderPass, RenderPassDescription&& renderPassDescription);
    ~VulkanRenderPass();

    void recreate(const VulkanDevice& device, const VkExtent2D& swapChainExtent);

    VkExtent2D getRenderArea() const;
    VkViewport createViewport() const;
    VkRect2D createScissor() const;

    void begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex, VkSubpassContents content) const;
    void end(VkCommandBuffer cmdBuffer, uint32_t frameIndex);
    void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

    VulkanImage& getRenderTarget(uint32_t index) const;
    const VulkanImageView& getRenderTargetView(uint32_t renderTargetIndex, uint32_t frameIndex) const;
    std::vector<VulkanImageView*> getRenderTargetViews(uint32_t renderTargetIndex) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(
        const VulkanDevice& device, uint32_t index, uint32_t numFrames) const;
    std::unique_ptr<VulkanImageView> createRenderTargetView(
        const VulkanDevice& device, uint32_t index, uint32_t baseLayer, uint32_t numLayers) const;

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

    std::unique_ptr<VulkanFramebuffer>& getFramebuffer(uint32_t frameIdx)
    {
        return m_framebuffers.at(frameIdx);
    }

    inline const std::string& getName() const
    {
        return m_name;
    }

    inline void setName(std::string name)
    {
        m_name = std::move(name);
    }

    void updateInitialLayouts(VkCommandBuffer cmdBuffer);

protected:
    void createRenderTargets(const VulkanDevice& device);
    void createRenderTargetViewsAndFramebuffers(const VulkanDevice& device);

    VkExtent3D getRenderAreaExtent() const;

    void createResources(const VulkanDevice& device);
    void freeResources();

    void setDepthRenderTargetInfo(
        uint32_t index, VkImageUsageFlags additionalFlags, VkClearDepthStencilValue clearValue = {0.0f, 0});
    void setColorRenderTargetInfo(
        uint32_t index, VkImageUsageFlags additionalFlags, VkClearColorValue clearValue = {0.0f, 0.0f, 0.0f, 0.0f});

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

    std::vector<AttachmentMapping> m_attachmentMappings;

    std::string m_name;

    bool m_combinedImages{false};
    bool m_bufferedRenderTargets{true};
    bool m_allocateRenderTargets{true};
};
} // namespace crisp
