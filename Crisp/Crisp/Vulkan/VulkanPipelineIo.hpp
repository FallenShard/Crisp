#pragma once

#include <filesystem>
#include <memory>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRasterizationPassDescriptor.hpp>

namespace crisp {

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFile(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    const VulkanDevice& device,
    const VulkanRasterizationPassDescriptor& rasterizationPassDescriptor,
    VkDescriptorSetLayout bindlessDescriptorSetLayout);

} // namespace crisp
