#pragma once

#include <filesystem>
#include <functional>
#include <span>

#include <Crisp/Math/Headers.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/PipelineLayoutBuilder.hpp>

namespace crisp {
std::unique_ptr<VulkanPipeline> createComputePipeline(
    const VulkanDevice& device,
    const std::filesystem::path& spvPath,
    const VkExtent3D& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride = {},
    std::span<const uint32_t> specializationConstants = {});

VkExtent3D getWorkGroupSize(const VulkanPipeline& pipeline);

VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline);
VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VkExtent3D& workGroupSize);

} // namespace crisp
