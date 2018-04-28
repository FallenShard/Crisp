#include "VarianceShadowMapPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    VarianceShadowMapPipeline::VarianceShadowMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 1, renderPass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Renderer::NumVirtualFrames }
        }, 1);

        m_vertShader = renderer->getShaderModule("variance-shadow-map-vert");
        m_fragShader = renderer->getShaderModule("variance-shadow-map-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void VarianceShadowMapPipeline::create(int width, int height)
    {
        m_handle = PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setViewport(m_renderPass->createViewport())
            .setScissor(m_renderPass->createScissor())
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }
}