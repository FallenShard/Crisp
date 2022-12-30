#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Vulkan/VulkanPipeline.hpp>
#include <Crisp/Vulkan/VulkanRenderPass.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace crisp
{
class Renderer;

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJson(
    const nlohmann::json& pipelineJson,
    Renderer& renderer,
    const VulkanRenderPass& renderPass,
    uint32_t subpassIndex = 0);

Result<std::unique_ptr<VulkanPipeline>> createPipelineFromJsonPath(
    const std::filesystem::path& path,
    Renderer& renderer,
    const VulkanRenderPass& renderPass,
    uint32_t subpassIndex = 0);

} // namespace crisp