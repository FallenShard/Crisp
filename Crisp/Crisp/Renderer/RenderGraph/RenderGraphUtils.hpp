#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanCommandBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>

#include <Crisp/Renderer/RenderGraph/RenderGraphHandles.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphResource.hpp>

namespace crisp {

enum class PassType {
    Compute,
    Rasterizer,
    RayTracing,
};

struct RenderPassExecutionContext {
    const VulkanCommandBuffer cmdBuffer;
    uint32_t virtualFrameIndex;
};

struct RenderGraphPass {
    std::string name; // Symbolic name for the render pass. Must be unique in the render graph.
    PassType type{PassType::Rasterizer};

    // Specified by builder during construction phase.
    std::vector<RenderGraphResourceHandle> inputs;  // Input resources for the pass (either attached or in shader).
    std::vector<ResourceAccessState> inputAccesses; // How the input resource is accessed in this pass.

    std::vector<RenderGraphResourceHandle> outputs; // Outputs for the pass.

    // Computed during compilation phase.
    std::vector<RenderGraphResourceHandle> colorAttachments;
    std::optional<RenderGraphResourceHandle> depthStencilAttachment;

    uint32_t getAttachmentCount() const {
        return static_cast<uint32_t>(colorAttachments.size() + (depthStencilAttachment.has_value() ? 1 : 0));
    };

    std::function<void(const RenderPassExecutionContext&)> executeFunc{};
};

enum class SizePolicy { Absolute, SwapChainRelative, InputRelative };

struct RenderGraphImageDescription {
    SizePolicy sizePolicy{SizePolicy::Absolute};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t depth{1};

    VkFormat format{VK_FORMAT_UNDEFINED};

    uint32_t layerCount{1};
    uint32_t mipLevelCount{1};

    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};

    VkImageCreateFlags createFlags{};

    std::optional<VkClearValue> clearValue{};
    VkImageUsageFlags imageUsageFlags{};

    bool canAlias(const RenderGraphImageDescription& desc) const {
        return sizePolicy == desc.sizePolicy && width == desc.height && depth == desc.depth && format == desc.format &&
               layerCount == desc.layerCount && mipLevelCount == desc.mipLevelCount &&
               sampleCount == desc.sampleCount && createFlags == desc.createFlags;
    }
};

struct RenderGraphBufferDescription {
    VkFormat formatHint;
    VkDeviceSize size;
    VkBufferUsageFlags usageFlags;
    VkBuffer externalBuffer;

    bool canAlias(const RenderGraphBufferDescription& desc) const {
        return size == desc.size && formatHint == desc.formatHint;
    }
};

inline VkExtent3D calculateImageExtent(const RenderGraphImageDescription& desc, const VkExtent2D swapChainExtent) {
    switch (desc.sizePolicy) {
    case SizePolicy::Absolute:
        CRISP_CHECK_GT(desc.width, 0);
        CRISP_CHECK_GT(desc.height, 0);
        CRISP_CHECK_GT(desc.depth, 0);
        return {desc.width, desc.height, desc.depth};
    default:
        CRISP_CHECK_GT(desc.depth, 0);
        return {swapChainExtent.width, swapChainExtent.height, desc.depth};
    }
}

struct RenderGraphPhysicalImage {
    uint32_t descriptionIndex{}; // Index into the descriptions for metadata.
    std::unique_ptr<VulkanImage> image;
    std::vector<uint32_t> aliasedResourceIndices;
};

struct RenderGraphPhysicalBuffer {
    uint32_t descriptionIndex{};
    std::unique_ptr<VulkanBuffer> buffer;
    std::vector<uint32_t> aliasedResourceIndices;
};

struct RenderGraphImportedBuffer {
    uint32_t descriptionIndex{};
    VulkanBuffer* buffer{nullptr};
};

} // namespace crisp