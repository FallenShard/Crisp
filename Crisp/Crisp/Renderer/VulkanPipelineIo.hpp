#pragma once

#include <memory>

#include <nlohmann/json.hpp>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/ShaderCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

namespace crisp {

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromFile(
    const std::filesystem::path& path,
    const std::filesystem::path& spvShaderDir,
    ShaderCache& shaderCache,
    const VulkanDevice& device,
    const VulkanRenderPass& renderPass,
    uint32_t subpassIndex = 0);

} // namespace crisp