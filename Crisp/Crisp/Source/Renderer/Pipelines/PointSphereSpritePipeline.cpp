#include "PointSphereSpritePipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    PointSphereSpritePipeline::PointSphereSpritePipeline(Renderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 2, renderPass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT }
        });

        m_descriptorSetLayouts[1] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts);

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
        }, 2);

        m_vertShader = renderer->getShaderModule("point-sphere-sprite-vert");
        m_fragShader = renderer->getShaderModule("point-sphere-sprite-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void PointSphereSpritePipeline::create(int width, int height)
    {
        m_handle = PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexInputBinding<1, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<1, 1, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            .setViewport(m_renderer->getDefaultViewport())
            .setScissor(m_renderer->getDefaultScissor())
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }
}