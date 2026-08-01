#pragma once

#include <span>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphBlackboard.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphUtils.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>
#include <Crisp/Vulkan/Rhi/VulkanTimestampQueryPool.hpp>

namespace crisp::rg {
namespace detail {
// Unevaluated helper: deduces the class type from a pointer-to-member.
// Used to enable `rg.getImageView<&Foo::bar>()` syntax without spelling out `Foo`.
template <typename C, typename M>
C inferMemberClass(M C::*);
} // namespace detail

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

        RenderGraphResourceHandle createStorageImage(const RenderGraphImageDescription& description, std::string&& name);

        RenderGraphResourceHandle importBuffer(const RenderGraphBufferDescription& description, std::string&& name);
        RenderGraphResourceHandle createBuffer(const RenderGraphBufferDescription& description, std::string&& name);

        RenderGraphResourceHandle writeAttachment(RenderGraphResourceHandle handle);

        RenderGraphBlackboard& getBlackboard();

        void setType(PassType type);

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

    std::unique_ptr<VulkanImageView> createViewFromResource(
        const VulkanDevice& device, RenderGraphResourceHandle handle) const;
    const VulkanImageView& getResourceImageView(RenderGraphResourceHandle handle) const;

    // Convenience: fetch a blackboard-published handle by member pointer and resolve to its view.
    // Usage: rg.getImageView<&ForwardLightingPassData::hdrImage>()
    template <auto Member>
    const VulkanImageView& getImageView() const {
        using ClassType = decltype(detail::inferMemberClass(Member));
        return getResourceImageView(m_blackboard.get<ClassType>().*Member);
    }

    const RenderGraphBlackboard& getBlackboard() const;

    VkExtent2D getRenderArea(const RenderGraphPass& pass, VkExtent2D swapChainExtent);

    void compile(const VulkanDevice& device, const VkExtent2D& swapChainExtent);

    void execute(const FrameContext& frameContext);

    RenderGraphBlackboard& getBlackboard();

    VulkanRasterizationPassDescriptor getRasterizationPassDescriptor(RenderGraphPassHandle passHandle) const;
    VulkanRasterizationPassDescriptor getRasterizationPassDescriptor(const std::string& name) const;

    void resize(const VulkanDevice& device, VkExtent2D swapChainExtent);

    const std::vector<RenderGraphResource>& getResources() const {
        return m_resources;
    }

    const std::vector<RenderGraphPass>& getPasses() const {
        return m_passes;
    }

    const RenderGraphPass& getPass(const RenderGraphPassHandle handle) const {
        return m_passes.at(handle.id);
    }

    const VulkanImageView& getImageView(const std::string& name, uint32_t attachmentIndex) const;

    std::span<const std::optional<double>> getGpuPassTimingsMs() const {
        return m_passProfiler.passTimingsMs;
    }

    std::optional<double> getGpuFrameTimingMs() const {
        return m_passProfiler.graphTimingMs;
    }

    bool isGpuProfilingSupported() const {
        return m_passProfiler.queryCount > 0;
    }

    const RenderGraphImageDescription& getImageDescription(const RenderGraphResourceHandle handle) const {
        return m_imageDescriptions.at(getResource(handle).descriptionIndex);
    }

    const RenderGraphBufferDescription& getBufferDescription(const RenderGraphResourceHandle handle) const {
        return m_bufferDescriptions.at(getResource(handle).descriptionIndex);
    }

    VkExtent3D getImageExtent(const RenderGraphResourceHandle handle) const {
        return calculateImageExtent(getImageDescription(handle), m_swapChainExtent);
    }

    size_t getPhysicalImageCount() const {
        return m_physicalImages.size();
    }

    size_t getPhysicalBufferCount() const {
        return m_physicalBuffers.size();
    }

private:
    struct ResourceTimeline {
        uint32_t firstWrite{~0u};
        uint32_t lastRead{0u};
    };

    std::vector<ResourceTimeline> calculateResourceTimelines();
    RenderGraphResourceHandle addImageResource(const RenderGraphImageDescription& description, std::string&& name);
    RenderGraphResourceHandle addBufferResource(
        const RenderGraphBufferDescription& description, std::string&& name, bool isExternal);

    RenderGraphPass& getPass(const RenderGraphPassHandle handle) {
        return m_passes.at(handle.id);
    }

    RenderGraphResource& getResource(const RenderGraphResourceHandle handle) {
        return m_resources.at(handle.id);
    }

    const RenderGraphResource& getResource(const RenderGraphResourceHandle handle) const {
        return m_resources.at(handle.id);
    }

    RenderGraphImageDescription& getImageDescription(const RenderGraphResourceHandle handle) {
        return m_imageDescriptions.at(getResource(handle).descriptionIndex);
    }

    VkImageUsageFlags determineUsageFlags(const std::vector<uint32_t>& imageResourceIndices) const;
    VkImageCreateFlags determineCreateFlags(const std::vector<uint32_t>& imageResourceIndices) const;
    std::pair<VkImageLayout, VulkanSynchronizationStage> determineInitialLayout(
        const RenderGraphPhysicalImage& image, VkImageUsageFlags usageFlags);

    void determineAliasedResurces();
    void createPhysicalResources(const VulkanDevice& device, VkExtent2D swapChainExtent, VkCommandBuffer cmdBuffer);

    struct PassProfiler {
        struct Frame {
            std::unique_ptr<VulkanTimestampQueryPool> queryPool;
            std::vector<uint64_t> timestamps;
            bool pending{false};
        };

        const VulkanDevice* device{nullptr};
        std::vector<Frame> frames;
        std::vector<std::optional<double>> passTimingsMs;
        std::optional<double> graphTimingMs;
        uint32_t queryCount{0};

        void initialize(const VulkanDevice& vulkanDevice, size_t passCount);
        Frame* beginFrame(uint32_t virtualFrameIndex);

        void endFrame(Frame* frame) const { // NOLINT
            if (frame) {
                frame->pending = true;
            }
        }
    };

    // The list of resources used by the render graph.
    std::vector<RenderGraphResource> m_resources;

    // The list of passes that are part of the render graph.
    std::vector<RenderGraphPass> m_passes;
    FlatStringHashMap<RenderGraphPassHandle> m_passMap;

    std::vector<RenderGraphImageDescription> m_imageDescriptions;
    std::vector<RenderGraphBufferDescription> m_bufferDescriptions;

    // Physical resources, built during compilation.
    std::vector<RenderGraphPhysicalImage> m_physicalImages;
    std::vector<RenderGraphPhysicalBuffer> m_physicalBuffers;
    std::vector<RenderGraphImportedBuffer> m_importedBuffers;

    FlatHashMap<uint32_t, std::unique_ptr<VulkanImageView>> m_imageViews;

    // Used to facilitate communication of pass dependencies across the codebase.
    RenderGraphBlackboard m_blackboard;

    VkExtent2D m_swapChainExtent{};
    PassProfiler m_passProfiler;
};
} // namespace crisp::rg
