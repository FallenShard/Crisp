#pragma once

#include <Crisp/Vulkan/VulkanPipeline.hpp>

#include <Crisp/Math/Headers.hpp>

namespace crisp {
class Renderer;

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer* renderer,
    std::string&& shaderName,
    uint32_t numDynamicStorageBuffers,
    uint32_t numDescriptorSets,
    const glm::uvec3& workGroupSize);
std::unique_ptr<VulkanPipeline> createLightCullingComputePipeline(Renderer* renderer, const glm::uvec3& workGroupSize);

glm::uvec3 getWorkGroupSize(const VulkanPipeline& pipeline);

glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline);
glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const glm::uvec3& workGroupSize);

} // namespace crisp
