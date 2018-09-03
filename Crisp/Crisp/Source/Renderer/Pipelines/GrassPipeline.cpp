#include "GrassPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createGrassPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            //{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
            //{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
            //{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT }

        })
        .defineDescriptorSet(1,
        {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1, Renderer::NumVirtualFrames }, 1 + Renderer::NumVirtualFrames);
        auto layout = createPipelineLayout(device, layoutBuilder, descPool);

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("grass-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("grass-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexInputBinding<1, VK_VERTEX_INPUT_RATE_INSTANCE, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexAttributes<1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .setCullMode(VK_CULL_MODE_NONE)
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(device, std::move(layout), renderPass->getHandle(), 0);
    }
}