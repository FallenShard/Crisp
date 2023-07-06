#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/RenderGraphBlackboard.hpp>
#include <Crisp/Renderer/RenderGraphExperimentalUtils.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp::rg
{
class RenderGraph
{
public:
    class Builder
    {
    public:
        Builder(RenderGraph& renderGraph, RenderGraphPassHandle passHandle);

        void exportTexture(RenderGraphResourceHandle res);
        void readTexture(RenderGraphResourceHandle res);
        void readBuffer(RenderGraphResourceHandle res);
        void readAttachment(RenderGraphResourceHandle res);
        void readStorageImage(RenderGraphResourceHandle res);

        RenderGraphResourceHandle createAttachment(
            const RenderGraphImageDescription& description,
            std::string&& name,
            std::optional<VkClearValue> clearValue = std::nullopt);

        RenderGraphResourceHandle createStorageImage(
            const RenderGraphImageDescription& description, std::string&& name);

        RenderGraphResourceHandle createBuffer(const RenderGraphBufferDescription& description, std::string&& name);

        RenderGraphResourceHandle writeAttachment(RenderGraphResourceHandle handle);

        RenderGraphBlackboard& getBlackboard();

    private:
        RenderGraph& m_renderGraph;
        RenderGraphPassHandle m_passHandle;
        RenderGraphBlackboard& m_blackboard;
    };

    template <typename BuilderFunc, typename ExecuteFunc>
    RenderGraphPassHandle addPass(std::string name, BuilderFunc&& builderFunc, ExecuteFunc&& executeFunc)
    {
        if (m_passMap.contains(name))
        {
            CRISP_FATAL("RenderGraph already contains pass named '{}'.", name);
        }

        m_passes.emplace_back();
        m_passes.back().name = std::move(name);
        m_passes.back().executeFunc = executeFunc;
        const RenderGraphPassHandle handle{static_cast<uint32_t>(m_passes.size()) - 1};
        m_passMap.emplace(m_passes.back().name, handle);

        Builder builder(*this, handle);
        builderFunc(builder);
        return handle;
    }

    size_t getPassCount() const;
    size_t getResourceCount() const;

    std::unique_ptr<VulkanImageView> createViewFromResource(RenderGraphResourceHandle handle) const;

    Result<> toGraphViz(const std::string& path) const;

    const RenderGraphBlackboard& getBlackboard() const;

    class RenderPassBuilder
    {
    public:
        // Attachment configuration
        RenderPassBuilder& setAttachmentCount(uint32_t count);
        RenderPassBuilder& setAttachmentOps(
            uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
        RenderPassBuilder& setAttachmentStencilOps(
            uint32_t attachmentIndex, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp);
        RenderPassBuilder& setAttachmentLayouts(
            uint32_t attachmentIndex, VkImageLayout initialLayout, VkImageLayout finalLayout);

        RenderPassBuilder& setAttachmentFormat(
            uint32_t attachmentIndex, VkFormat format, VkSampleCountFlagBits sampleCount);

        // Subpass configuration
        RenderPassBuilder& setSubpassCount(uint32_t numSubpasses);
        RenderPassBuilder& configureSubpass(
            uint32_t subpass,
            VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            VkSubpassDescriptionFlags flags = 0);

        RenderPassBuilder& addInputAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
        RenderPassBuilder& addColorAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);
        RenderPassBuilder& addColorAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);
        RenderPassBuilder& addResolveAttachmentRef(
            uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);

        RenderPassBuilder& setDepthAttachmentRef(uint32_t subpass, uint32_t attachmentIndex, VkImageLayout imageLayout);

        RenderPassBuilder& addPreserveAttachmentRef(uint32_t subpass, uint32_t attachmentIndex);

        RenderPassBuilder& addDependency(
            uint32_t srcSubpass,
            uint32_t dstSubpass,
            VkPipelineStageFlags srcStageMask,
            VkAccessFlags srcAccessMask,
            VkPipelineStageFlags dstStageMask,
            VkAccessFlags dstAccessMask,
            VkDependencyFlags flags = 0);

        std::pair<VkRenderPass, std::vector<VkAttachmentDescription>> create(VkDevice device) const;
        std::vector<VkImageLayout> getFinalLayouts() const;

    private:
        std::vector<VkAttachmentDescription> m_attachments;

        std::vector<std::vector<VkAttachmentReference>> m_inputAttachmentRefs;
        std::vector<std::vector<VkAttachmentReference>> m_colorAttachmentRefs;
        std::vector<std::vector<VkAttachmentReference>> m_resolveAttachmentRefs;
        std::vector<VkAttachmentReference> m_depthAttachmentRefs;
        std::vector<std::vector<uint32_t>> m_preserveAttachments;
        std::vector<VkSubpassDescription> m_subpasses;

        std::vector<VkSubpassDependency> m_dependencies;
    };

    VkExtent2D getRenderArea(const RenderGraphPass& pass, VkExtent2D swapChainExtent);

    static RenderTargetInfo toRenderTargetInfo(const RenderGraphImageDescription& desc);

    void compile(const VulkanDevice& device, const VkExtent2D& swapChainExtent, VkCommandBuffer cmdBuffer);

    void execute(VkCommandBuffer cmdBuffer);

    void determineAliasedResurces();

    void createPhysicalResources(const VulkanDevice& device, VkExtent2D swapChainExtent, VkCommandBuffer cmdBuffer);

    RenderGraphBlackboard& getBlackboard();

    const VulkanRenderPass& getRenderPass(const std::string& name) const;

    void resize(uint32_t width, uint32_t height) const;

private:
    struct ResourceTimeline
    {
        uint32_t firstWrite{~0u};
        uint32_t lastRead{0};
    };

    std::vector<ResourceTimeline> calculateResourceTimelines();
    RenderGraphResourceHandle addImageResource(const RenderGraphImageDescription& description, std::string&& name);
    RenderGraphResourceHandle addBufferResource(const RenderGraphBufferDescription& description, std::string&& name);

    RenderGraphPass& getPass(RenderGraphPassHandle handle);
    const RenderGraphPass& getPass(RenderGraphPassHandle handle) const;

    RenderGraphResource& getResource(RenderGraphResourceHandle handle);
    const RenderGraphResource& getResource(RenderGraphResourceHandle handle) const;
    const RenderGraphImageDescription& getImageDescription(RenderGraphResourceHandle handle);

    // The list of resources used by the render graph.
    std::vector<RenderGraphResource> m_resources;

    // The list of passes that are part of the render graph.
    std::vector<RenderGraphPass> m_passes;
    FlatHashMap<std::string, RenderGraphPassHandle> m_passMap;

    std::vector<RenderGraphImageDescription> m_imageDescriptions;
    std::vector<RenderGraphBufferDescription> m_bufferDescriptions;

    // Physical resources, built during compilation.
    std::vector<RenderGraphPhysicalImage> m_physicalImages;
    std::vector<RenderGraphPhysicalBuffer> m_physicalBuffers;
    // std::vector<RenderGraphPhysicalPass> m_physicalPasses;
    std::vector<std::unique_ptr<VulkanRenderPass>> m_physicalPasses;

    // Used to facilitate communication of pass dependencies across the codebase.
    RenderGraphBlackboard m_blackboard;
};
} // namespace crisp::rg