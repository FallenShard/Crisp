#include "PointSphereSpritePipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createPointSphereSpritePipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
            })
            .defineDescriptorSet(1, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t));

        VulkanDevice* device = renderer->getDevice();

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("point-sphere-sprite-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("point-sphere-sprite-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexInputBinding<1, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<0, 0, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .addVertexAttributes<1, 1, VK_FORMAT_R32G32B32A32_SFLOAT>()
            .setInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
            .setDepthTestOperation(VK_COMPARE_OP_GREATER_OR_EQUAL)
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .create(device, layoutBuilder.create(device), renderPass->getHandle(), 0);
    }
}