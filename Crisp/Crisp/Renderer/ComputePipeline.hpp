#pragma once

#include <functional>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {
class Renderer;

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer& renderer,
    const std::string& shaderName,
    const VkExtent3D& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride = {});

VkExtent3D getWorkGroupSize(const VulkanPipeline& pipeline);

VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline);
VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VkExtent3D& workGroupSize);

} // namespace crisp
