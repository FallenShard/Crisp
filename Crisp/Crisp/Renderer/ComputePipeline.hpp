#pragma once

#include <filesystem>
#include <flat_map>
#include <functional>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Vulkan/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {

std::unique_ptr<VulkanPipeline> createComputePipeline(
    const VulkanDevice& device,
    const std::filesystem::path& spvPath,
    const VkExtent3D& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride = {},
    const SpecializationConstantMap& specializationConstants = {});

VkExtent3D getWorkGroupSize(const VulkanPipeline& pipeline);

VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline);
VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VkExtent3D& workGroupSize);

} // namespace crisp
