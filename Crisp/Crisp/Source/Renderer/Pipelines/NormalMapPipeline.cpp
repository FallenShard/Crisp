#include "NormalMapPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/PipelineBuilder.hpp"

namespace crisp
{
    NormalMapPipeline::NormalMapPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 3, renderPass)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },   // 1
            { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // 1
            { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT }  // 1
        });

        m_descriptorSetLayouts[1] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // 3
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }  // 3
        });

        m_descriptorSetLayouts[2] = createDescriptorSetLayout(
        {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT } // 1
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts, {
            { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 2 * sizeof(glm::mat4) },
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Renderer::NumVirtualFrames },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * Renderer::NumVirtualFrames + 1 }
        }, 2 + Renderer::NumVirtualFrames);

        m_vertShader = renderer->getShaderModule("normal-map-vert");
        m_fragShader = renderer->getShaderModule("normal-map-frag");

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);
    }

    void NormalMapPipeline::create(int width, int height)
    {
        m_handle = PipelineBuilder()
            .setShaderStages({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX,
                VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(m_renderer->getDefaultViewport())
            .setScissor(m_renderer->getDefaultScissor())
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }
}