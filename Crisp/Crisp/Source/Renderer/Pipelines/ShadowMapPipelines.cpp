#include "ShadowMapPipelines.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("shadow-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("shadow-map-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice()), renderPass->getHandle(), subpass);
    }

    std::unique_ptr<VulkanPipeline> createInstancingShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
        })
        .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        VulkanDevice* device = renderer->getDevice();

        std::vector<bool> setBuffered = { false };
        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1 }, 1);
        auto layout = createPipelineLayout(device, layoutBuilder, setBuffered, descPool);

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
            .create(device, std::move(layout), renderPass->getHandle(), subpass);
    }

    std::unique_ptr<VulkanPipeline> createVarianceShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("variance-shadow-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("variance-shadow-map-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice()), renderPass->getHandle(), 0);
    }
}