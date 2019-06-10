#include "TerrainPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createTerrainPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
                { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(float) + sizeof(float));

        VulkanDevice* device = renderer->getDevice();

        std::vector<bool> setBuffered = { false };
        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1 }, 1);
        auto layout   = createPipelineLayout(device, layoutBuilder, setBuffered, descPool);

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,                  renderer->getShaderModule("terrain-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,    renderer->getShaderModule("terrain-tesc")),
                createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, renderer->getShaderModule("terrain-tese")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT,                renderer->getShaderModule("terrain-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
            .setTessellationControlPoints(4)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(device, std::move(layout), renderPass->getHandle(), 0);
    }
}