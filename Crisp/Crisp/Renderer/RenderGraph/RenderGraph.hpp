#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphBlackboard.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphUtils.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

namespace crisp::rg {
class RenderGraph {
public:
    class Builder {
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
    };

    template <typename BuilderFunc, typename ExecuteFunc>
    RenderGraphPassHandle addPass(std::string name, const BuilderFunc& builderFunc, ExecuteFunc&& executeFunc) {
        if (m_passMap.contains(name)) {
            CRISP_FATAL("RenderGraph already contains pass named '{}'.", name);
        }

        m_passes.emplace_back();
        m_passes.back().name = std::move(name);
        m_passes.back().executeFunc = std::forward<ExecuteFunc>(executeFunc);
        const RenderGraphPassHandle handle{static_cast<uint32_t>(m_passes.size()) - 1};
        m_passMap.emplace(m_passes.back().name, handle);

        Builder builder(*this, handle);
        builderFunc(builder);
        return handle;
    }

    size_t getPassCount() const;
    size_t getResourceCount() const;

    std::unique_ptr<VulkanImageView> createViewFromResource(RenderGraphResourceHandle handle) const;

    const RenderGraphBlackboard& getBlackboard() const;

    VkExtent2D getRenderArea(const RenderGraphPass& pass, VkExtent2D swapChainExtent);

    void compile(const VulkanDevice& device, const VkExtent2D& swapChainExtent, VkCommandBuffer cmdBuffer);

    void execute(VkCommandBuffer cmdBuffer);

    RenderGraphBlackboard& getBlackboard();

    const VulkanRenderPass& getRenderPass(const std::string& name) const;

    void resize(const VulkanDevice& device, VkExtent2D swapChainExtent, VkCommandBuffer cmdBuffer);

    const std::vector<RenderGraphResource>& getResources() const {
        return m_resources;
    }

    const std::vector<RenderGraphPass>& getPasses() const {
        return m_passes;
    }

    const RenderGraphPass& getPass(const RenderGraphPassHandle handle) const {
        return m_passes.at(handle.id);
    }

private:
    struct ResourceTimeline {
        uint32_t firstWrite{~0u};
        uint32_t lastRead{0u};
    };

    std::vector<ResourceTimeline> calculateResourceTimelines();
    RenderGraphResourceHandle addImageResource(const RenderGraphImageDescription& description, std::string&& name);
    RenderGraphResourceHandle addBufferResource(const RenderGraphBufferDescription& description, std::string&& name);

    RenderGraphPass& getPass(const RenderGraphPassHandle handle) {
        return m_passes.at(handle.id);
    }

    RenderGraphResource& getResource(const RenderGraphResourceHandle handle) {
        return m_resources.at(handle.id);
    }

    const RenderGraphResource& getResource(const RenderGraphResourceHandle handle) const {
        return m_resources.at(handle.id);
    }

    const RenderGraphImageDescription& getImageDescription(const RenderGraphResourceHandle handle) const {
        return m_imageDescriptions.at(getResource(handle).descriptionIndex);
    }

    VkImageUsageFlags determineUsageFlags(const std::vector<uint32_t>& imageResourceIndices) const;
    VkImageCreateFlags determineCreateFlags(const std::vector<uint32_t>& imageResourceIndices) const;
    std::tuple<VkImageLayout, VkPipelineStageFlagBits> determineInitialLayout(
        const RenderGraphPhysicalImage& image, VkImageUsageFlags usageFlags);

    void determineAliasedResurces();
    void createPhysicalResources(const VulkanDevice& device, VkExtent2D swapChainExtent, VkCommandBuffer cmdBuffer);
    void createPhysicalPasses(const VulkanDevice& device, VkExtent2D swapChainExtent);

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
    std::vector<std::unique_ptr<VulkanRenderPass>> m_physicalPasses;

    // Used to facilitate communication of pass dependencies across the codebase.
    RenderGraphBlackboard m_blackboard;
};
} // namespace crisp::rg