#pragma once

#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {

struct VulkanSynchronizationStage {
    VkPipelineStageFlags2 stage;
    VkAccessFlags2 access;
};

struct VulkanSynchronizationScope {
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;

    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;

    constexpr VulkanSynchronizationStage src() const {
        return {srcStage, srcAccess};
    }

    constexpr VulkanSynchronizationStage dst() const {
        return {dstStage, dstAccess};
    }
};

constexpr VulkanSynchronizationScope operator>>(VulkanSynchronizationStage src, VulkanSynchronizationStage dst) {
    return {
        .srcStage = src.stage,
        .srcAccess = src.access,
        .dstStage = dst.stage,
        .dstAccess = dst.access,
    };
}

constexpr VulkanSynchronizationStage operator|(VulkanSynchronizationStage a, VulkanSynchronizationStage b) {
    return {
        .stage = a.stage | b.stage,
        .access = a.access | b.access,
    };
}

constexpr VulkanSynchronizationScope operator|(VulkanSynchronizationScope a, VulkanSynchronizationScope b) {
    return {
        .srcStage = a.srcStage | b.srcStage,
        .srcAccess = a.srcAccess | b.srcAccess,
        .dstStage = a.dstStage | b.dstStage,
        .dstAccess = a.dstAccess | b.dstAccess,
    };
}

inline constexpr VulkanSynchronizationStage kTransferWrite = {
    .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kTransferRead = {
    .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .access = VK_ACCESS_2_TRANSFER_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kComputeRead = {
    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .access = VK_ACCESS_2_SHADER_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kVertexRead = {
    .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    .access = VK_ACCESS_2_SHADER_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kVertexInputRead = {
    .stage = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
    .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kIndexInputRead = {
    .stage = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
    .access = VK_ACCESS_2_INDEX_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kVertexUniformRead = {
    .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
    .access = VK_ACCESS_2_UNIFORM_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kFragmentUniformRead = {
    .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .access = VK_ACCESS_2_UNIFORM_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kFragmentRead = {
    .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .access = VK_ACCESS_2_SHADER_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kRayTracingRead = {
    .stage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    .access = VK_ACCESS_2_SHADER_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kRayTracingStorageRead = {
    .stage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    .access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kRayTracingUniformRead = {
    .stage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    .access = VK_ACCESS_2_UNIFORM_READ_BIT,
};

inline constexpr auto kAllShaderRead = kComputeRead | kVertexRead | kFragmentRead | kRayTracingRead;

inline constexpr VulkanSynchronizationStage kComputeWrite = {
    .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    .access = VK_ACCESS_2_SHADER_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kColorWrite = {
    .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kDepthWrite = {
    .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kRayTracingStorageWrite = {
    .stage = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
    .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kFragmentSampledRead = {
    .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kFragmentInputRead = {
    .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .access = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
};

inline constexpr VulkanSynchronizationStage kExternalColorSubpass = {
    .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .access = VK_ACCESS_2_NONE,
};

inline constexpr VulkanSynchronizationStage kNullStage = {
    .stage = VK_PIPELINE_STAGE_2_NONE,
    .access = VK_ACCESS_2_NONE,
};

inline constexpr VulkanSynchronizationStage kAllStages = {
    .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
};

inline constexpr VulkanSynchronizationStage kHostWrite = {
    .stage = VK_PIPELINE_STAGE_2_HOST_BIT,
    .access = VK_ACCESS_2_HOST_WRITE_BIT,
};

} // namespace crisp