#pragma once

#include <string>
#include <vector>

#include <Crisp/Renderer/RenderGraph/RenderGraphHandles.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanSynchronization.hpp>

namespace crisp {

enum class ResourceType : uint8_t {
    Buffer,
    Image,
    Unknown,
};

enum class ResourceUsageType : uint8_t {
    Storage,    // For compute, non-sampled use.
    Attachment, // For render pass attachments/render targets.
    Texture,    // For sampled images.
};

struct ResourceAccessState {
    ResourceUsageType usageType;
    VulkanSynchronizationStage stage;
};

struct RenderGraphResource {
    static constexpr uint16_t kInvalidIndex{std::numeric_limits<uint16_t>::max()};

    // Buffer, Image, or something else in the future. Determines the array to index into with description index.
    ResourceType type{ResourceType::Unknown};
    uint16_t version{0};                           // Version to keep track of read-modify-write resources.
    std::vector<RenderGraphPassHandle> readPasses; // Render pass handles where this resource is read.

    // Index into resource description - a minimal set of input parameters to describe the resource.
    uint16_t descriptionIndex{kInvalidIndex};

    // Index into the array that holds the actual physical resource - a VkImage or a VkBuffer.
    uint16_t physicalResourceIndex{kInvalidIndex};

    RenderGraphPassHandle producer;     // Pass that created and/or wrote to this resource.
    ResourceAccessState producerAccess; // How the producer created this resource.

    std::string name; // Symbolic name of the resource. Useful for debugging and logging.

    bool isExternal{false}; // Indicates whether the resource is managed externally.
};

} // namespace crisp