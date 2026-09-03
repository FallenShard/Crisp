#pragma once

#include <filesystem>
#include <memory>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Vulkan/PipelineBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>

namespace crisp {

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFile(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    const VulkanDevice& device,
    VkDescriptorSetLayout bindlessDescriptorSetLayout,
    const VulkanPipelineParams& params);

} // namespace crisp
