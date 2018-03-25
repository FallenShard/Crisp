#include "ColorAndShadowPipeline.hpp"

#include "Vulkan/VulkanFormatTraits.hpp"
#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    ColorAndShadowPipeline::ColorAndShadowPipeline(VulkanRenderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 2, renderPass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_descriptorSetLayouts[1] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts, {
            { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 2 * sizeof(glm::mat4) },
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VulkanRenderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * VulkanRenderer::NumVirtualFrames }
        }, 1 + VulkanRenderer::NumVirtualFrames);

        m_vertShader = renderer->getShaderModule("color-and-shadow-vert");
        m_fragShader = renderer->getShaderModule("color-and-shadow-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void ColorAndShadowPipeline::create(int width, int height)
    {
        m_handle = PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .setViewport(m_renderer->getDefaultViewport())
            .setScissor(m_renderer->getDefaultScissor())
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }
}