#include "ShadowMapPipelines.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createInstancingShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
        })
        .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        VulkanDevice* device = renderer->getDevice();

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("shadow-map-instancing-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("shadow-map-instancing-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexInputBinding<1, VK_VERTEX_INPUT_RATE_INSTANCE, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexAttributes<1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .setCullMode(VK_CULL_MODE_NONE)
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(device, layoutBuilder.create(device), renderPass->getHandle(), subpass);
    }
}