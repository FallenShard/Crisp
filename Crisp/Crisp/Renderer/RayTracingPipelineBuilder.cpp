#include <Crisp/Renderer/RayTracingPipelineBuilder.hpp>

#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>
#include <Crisp/Vulkan/VulkanHeader.hpp>

namespace crisp {
RayTracingPipelineBuilder::RayTracingPipelineBuilder(Renderer& renderer)
    : m_renderer(renderer) {}

void RayTracingPipelineBuilder::addShaderStage(const std::string& shaderName) {
    m_stages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = getShaderStageFromFilePath(shaderName).unwrap(),
        .module = m_renderer.getOrLoadShaderModule(shaderName),
        .pName = "main",
    });
}

void RayTracingPipelineBuilder::addShaderGroup(
    const uint32_t shaderStageIdx, const VkRayTracingShaderGroupTypeKHR type) {
    m_groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = type,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    });
    switch (type) {
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR: {
        m_groups.back().generalShader = shaderStageIdx;
        break;
    }
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR: {
        m_groups.back().closestHitShader = shaderStageIdx;
        break;
    }
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR: {
        m_groups.back().intersectionShader = shaderStageIdx;
        break;
    }
    default: {
    }
    }
}

VkPipeline RayTracingPipelineBuilder::createHandle(const VkPipelineLayout pipelineLayout) {
    VkRayTracingPipelineCreateInfoKHR raytracingCreateInfo = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    raytracingCreateInfo.stageCount = static_cast<uint32_t>(m_stages.size());
    raytracingCreateInfo.pStages = m_stages.data();
    raytracingCreateInfo.groupCount = static_cast<uint32_t>(m_groups.size());
    raytracingCreateInfo.pGroups = m_groups.data();
    raytracingCreateInfo.layout = pipelineLayout;
    raytracingCreateInfo.maxPipelineRayRecursionDepth = 1; // We set up iterative ray tracing in the generation shader.
    VkPipeline pipeline{VK_NULL_HANDLE};
    vkCreateRayTracingPipelinesKHR(
        m_renderer.getDevice().getHandle(), {}, nullptr, 1, &raytracingCreateInfo, nullptr, &pipeline);
    return pipeline;
}

std::unique_ptr<VulkanBuffer> RayTracingPipelineBuilder::createShaderBindingTable(const VkPipeline rayTracingPipeline) {
    const auto createShaderHandleBuffer = [this](const VkPipeline rayTracingPipeline, const uint32_t groupCount) {
        const uint32_t handleSize =
            m_renderer.getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupHandleSize;
        const uint32_t baseAlignment =
            m_renderer.getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;
        std::vector<uint8_t> shaderHandleBuffer(groupCount * baseAlignment); // NOLINT

        vkGetRayTracingShaderGroupHandlesKHR(
            m_renderer.getDevice().getHandle(),
            rayTracingPipeline,
            0,
            groupCount,
            shaderHandleBuffer.size(),
            shaderHandleBuffer.data());

        for (int32_t i = static_cast<int32_t>(groupCount) - 1; i >= 0; --i) {
            memcpy(&shaderHandleBuffer[i * baseAlignment], &shaderHandleBuffer[i * handleSize], handleSize); // NOLINT
        }
        return shaderHandleBuffer;
    };

    std::vector<uint8_t> shaderHandleStorage(
        createShaderHandleBuffer(rayTracingPipeline, static_cast<uint32_t>(m_groups.size())));
    auto buffer = std::make_unique<VulkanBuffer>(
        m_renderer.getDevice(),
        shaderHandleStorage.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    m_renderer.enqueueResourceUpdate(
        [buffer = buffer.get(), handleStorage = std::move(shaderHandleStorage)](VkCommandBuffer cmdBuffer) {
            vkCmdUpdateBuffer(cmdBuffer, buffer->getHandle(), 0, handleStorage.size(), handleStorage.data());
        });

    return std::move(buffer);
}
} // namespace crisp