#include "BlurPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createBlurPipeline(Renderer* renderer, VulkanRenderPass* renderPass)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0,
            {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 3 * sizeof(float) + sizeof(int));

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { Renderer::NumVirtualFrames }, Renderer::NumVirtualFrames);
        auto layout   = createPipelineLayout(device, layoutBuilder, descPool);

        VkPipeline pipeline = PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   renderer->getShaderModule("blur-vert")),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, renderer->getShaderModule("blur-frag"))
            })
            .setFullScreenVertexLayout()
            .setViewport(renderPass->createViewport())
            .setScissor(renderPass->createScissor())
            .create(device->getHandle(), layout->getHandle(), renderPass->getHandle(), 0);

        return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout));
    }
}