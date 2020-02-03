#include "NormalMapPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createNormalMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .defineDescriptorSet(1, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .defineDescriptorSet(2, true,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            });

        return PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("normal-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("normal-map-frag"))
             })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
                VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice(), 10), renderPass->getHandle(), 0);
    }

    std::unique_ptr<VulkanPipeline> createTextureMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .defineDescriptorSet(1, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .defineDescriptorSet(2, true,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            });

        return PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("normal-map-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("normal-map-frag"))
             })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
                VK_FORMAT_R32G32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT>()
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(renderer->getDevice(), layoutBuilder.create(renderer->getDevice(), 10), renderPass->getHandle(), 0);
    }
}