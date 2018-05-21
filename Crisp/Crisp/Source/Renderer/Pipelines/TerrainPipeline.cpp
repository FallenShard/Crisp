#include "TerrainPipeline.hpp"

#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"
#include "Vulkan/VulkanDevice.hpp"

namespace crisp
{
    TerrainPipeline::TerrainPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
        : VulkanPipeline(renderer, 1, renderPass, false)
    {
        m_descriptorSetLayouts[0] = createDescriptorSetLayout({
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
            { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT}
        });

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts,
        {
            { VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(float) + sizeof(float) }
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
        }, 1);

        m_vertShader = renderer->getShaderModule("terrain-vert");
        m_tescShader = renderer->getShaderModule("terrain-tesc");
        m_teseShader = renderer->getShaderModule("terrain-tese");
        m_fragShader = renderer->getShaderModule("terrain-frag");

        m_handle = PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, m_vertShader),
                createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, m_tescShader),
                createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_teseShader),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader)
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .setTessellationControlPoints(4)
            .setPolygonMode(VK_POLYGON_MODE_LINE)
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(m_renderer->getDevice()->getHandle(), m_pipelineLayout, m_renderPass->getHandle(), 0);
    }

    void TerrainPipeline::create(int width, int height)
    {
    }
}