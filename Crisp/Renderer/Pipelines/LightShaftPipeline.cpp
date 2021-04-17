#include "LightShaftPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createLightShaftPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, true,
            {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            });

        VulkanDevice* device = renderer->getDevice();

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("light-shaft.vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("light-shaft.frag"))
            })
            .setFullScreenVertexLayout()
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .setBlendState(0, VK_TRUE)
            .setBlendFactors(0, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE)
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .setDepthTest(VK_FALSE)
            .setDepthWrite(VK_FALSE)
            .create(device, layoutBuilder.create(device), renderPass->getHandle(), 0);
    }
}