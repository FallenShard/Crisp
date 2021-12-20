#pragma once

#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
std::unique_ptr<VulkanPipeline> createComputePipeline(Renderer* renderer, std::string&& shaderName,
    uint32_t numDynamicStorageBuffers, uint32_t numDescriptorSets, std::size_t pushConstantSize,
    const glm::uvec3& workGroupSize);
std::unique_ptr<VulkanPipeline> createLightCullingComputePipeline(Renderer* renderer, const glm::uvec3& workGroupSize);

glm::uvec3 getWorkGroupSize(const VulkanPipeline& pipeline);
} // namespace crisp
