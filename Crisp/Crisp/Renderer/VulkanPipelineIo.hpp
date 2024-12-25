#pragma once

#include <Crisp/Core/Result.hpp>
#include <Crisp/Renderer/ShaderCache.hpp>
#include <Crisp/Vulkan/Rhi/VulkanPipeline.hpp>
#include <Crisp/Vulkan/Rhi/VulkanRenderPass.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace crisp {
class Renderer;

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJson(
    const nlohmann::json& pipelineJson,
    Renderer& renderer,
    const VulkanRenderPass& renderPass,
    uint32_t subpassIndex = 0);

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJsonPath(
    const std::filesystem::path& path, Renderer& renderer, const VulkanRenderPass& renderPass, uint32_t subpassIndex = 0);

} // namespace crisp