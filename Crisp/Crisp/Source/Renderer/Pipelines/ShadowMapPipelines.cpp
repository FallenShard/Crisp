#include "ShadowMapPipelines.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1 }, 1);
        auto layout   = createPipelineLayout(device, layoutBuilder, descPool);

        VkPipeline pipeline = PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("shadow-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("shadow-map-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(device->getHandle(), layout->getHandle(), renderPass->getHandle(), subpass);

        return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout));
    }

    std::unique_ptr<VulkanPipeline> createVarianceShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
            });

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1 }, 1);
        auto layout   = createPipelineLayout(device, layoutBuilder, descPool);

        VkPipeline pipeline = PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("variance-shadow-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("variance-shadow-map-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(device->getHandle(), layout->getHandle(), renderPass->getHandle(), 0);

        return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout));
    }
}