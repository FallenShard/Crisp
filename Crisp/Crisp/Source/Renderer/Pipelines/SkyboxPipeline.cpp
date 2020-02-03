#include "SkyboxPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderPass.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createSkyboxPipeline(Renderer* renderer, const VulkanRenderPass& renderPass, uint32_t subpass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, false,
            {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
                { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            });

        VulkanDevice* device = renderer->getDevice();

        return PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("skybox-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("skybox-frag"))
            })
            .addVertexInputBinding<0, VK_VERTEX_INPUT_RATE_VERTEX, VK_FORMAT_R32G32B32_SFLOAT>()
            .setVertexAttributes<VK_FORMAT_R32G32B32_SFLOAT>()
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
            .create(device, layoutBuilder.create(device), renderPass.getHandle(), subpass);
    }
}