#include "ShadowMapPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    ShadowMapPipeline::ShadowMapPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass)
        : VulkanPipeline(renderer, 1, renderPass)
        , m_subpass(subpass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
        });

        std::vector<VkPushConstantRange> pushConstants =
        {
            { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) }
        };

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts, pushConstants);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 }
        }, 1);

        m_vertShader = renderer->getShaderModule("shadow-map-vert");
        m_fragShader = renderer->getShaderModule("shadow-map-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void ShadowMapPipeline::create(int width, int height)
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
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), m_subpass);
    }
}