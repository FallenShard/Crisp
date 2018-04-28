#include "BlurPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    BlurPipeline::BlurPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 1, renderPass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts,
        {
            { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 3 * sizeof(float) + sizeof(int) }
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Renderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Renderer::NumVirtualFrames },
        }, Renderer::NumVirtualFrames);

        m_vertShader = renderer->getShaderModule("blur-vert");
        m_fragShader = renderer->getShaderModule("blur-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void BlurPipeline::create(int width, int height)
    {
        m_handle = PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32_SFLOAT>()
            .setViewport(m_renderPass->createViewport())
            .setScissor(m_renderPass->createScissor())
            .create(m_device->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }
}