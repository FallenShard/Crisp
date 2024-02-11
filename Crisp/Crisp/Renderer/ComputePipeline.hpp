#pragma once

#include <functional>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>

namespace crisp {
class Renderer;

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer& renderer,
    const std::string& shaderName,
    const glm::uvec3& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride = {});

glm::uvec3 getWorkGroupSize(const VulkanPipeline& pipeline);

glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline);
glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const glm::uvec3& workGroupSize);

} // namespace crisp
