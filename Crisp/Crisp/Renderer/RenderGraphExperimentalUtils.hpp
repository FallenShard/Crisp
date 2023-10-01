#pragma once

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>

namespace crisp {

#define STRINGIFY(x) #x

#define APPEND_FLAG_BIT_STR(res, flags, bit)                                                                           \
    if ((flags) & VK_IMAGE_USAGE_##bit) {                                                                              \
        (res) += STRINGIFY(VK_IMAGE_USAGE_##bit);                                                                      \
        (res) += " | ";                                                                                                \
    }

inline std::string toString(VkImageUsageFlags flags) {
    std::string res;
    APPEND_FLAG_BIT_STR(res, flags, TRANSFER_SRC_BIT);
    APPEND_FLAG_BIT_STR(res, flags, TRANSFER_DST_BIT);
    APPEND_FLAG_BIT_STR(res, flags, SAMPLED_BIT);
    APPEND_FLAG_BIT_STR(res, flags, STORAGE_BIT);
    APPEND_FLAG_BIT_STR(res, flags, COLOR_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, DEPTH_STENCIL_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, TRANSIENT_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, INPUT_ATTACHMENT_BIT);
    APPEND_FLAG_BIT_STR(res, flags, FRAGMENT_DENSITY_MAP_BIT_EXT);
    APPEND_FLAG_BIT_STR(res, flags, FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);
    return res;
}

struct RenderGraphPassHandle {
    static constexpr uint32_t kInvalidId{~0u};
    static constexpr uint32_t kExternalPass{~0u};

    uint32_t id{kInvalidId};
};

struct RenderGraphResourceHandle {
    static constexpr uint32_t kInvalidId{~0u};

    uint32_t id{kInvalidId};
};

enum class ResourceType { Buffer, Image, Unknown };

struct RenderGraphResource {
    static constexpr uint16_t kInvalidIndex{std::numeric_limits<uint16_t>::max()};

    // Buffer, Image, or something else in the future. Determines the array to index into with description index.
    ResourceType type{ResourceType::Unknown};
    uint16_t version{0};                           // Version to keep track of read-modify-write resources.
    std::vector<RenderGraphPassHandle> readPasses; // Render pass handles where this resource is read.

    uint16_t descriptionIndex{kInvalidIndex}; // Index into resource description - a minimal set of input parameters
                                              // to describe the resource.
    uint16_t physicalResourceIndex{kInvalidIndex};

    RenderGraphPassHandle producer; // Pass that created and/or wrote to this resource.

    std::optional<VkClearValue> clearValue{};
    VkImageUsageFlags imageUsageFlags{};

    std::string name; // Symbolic name of the resource.
};

enum class PassType { Compute, Rasterizer, RayTracing, Unknown };

enum class ResourceUsageType { Storage, Attachment, Texture };

struct ResourceAccessState {
    ResourceUsageType usageType;
    VkPipelineStageFlags pipelineStage;
    VkAccessFlags access;
};

struct RenderPassExecutionContext {
    VkCommandBuffer cmdBuffer;
};

struct RenderGraphPass {
    std::string name; // Symbolic name for the render pass. Must be unique in the render graph.
    PassType type{PassType::Unknown};

    // Specified by builder during construction phase.
    std::vector<RenderGraphResourceHandle> inputs;  // Input resources for the pass (either attached or in shader).
    std::vector<ResourceAccessState> inputAccesses; // How the input resource is accessed in this pass.

    std::vector<RenderGraphResourceHandle> outputs;  // Outputs for the pass.
    std::vector<ResourceAccessState> outputAccesses; // How the output resource is accessed in this pass.

    // Computed during compilation phase.
    std::vector<RenderGraphPassHandle> edges; // Other passes that have a data dependency with this pass.
    std::vector<RenderGraphResourceHandle> colorAttachments;
    std::optional<RenderGraphResourceHandle> depthStencilAttachment;

    void pushColorAttachment(RenderGraphResourceHandle handle) {
        colorAttachments.push_back(handle);
    }

    void setDepthAttachment(RenderGraphResourceHandle handle) {
        depthStencilAttachment = handle;
    }

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

} // namespace crisp