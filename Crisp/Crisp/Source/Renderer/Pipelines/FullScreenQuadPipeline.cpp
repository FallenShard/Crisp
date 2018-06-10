#include "FullScreenQuadPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unique_ptr<VulkanPipeline> createTonemappingPipeline(Renderer* renderer, VulkanRenderPass* renderPass, uint32_t subpass, bool useGammaCorrection)
    {
        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder
            .defineDescriptorSet(0,
            {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            })
            .addPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int));

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { 1 }, 1);
        auto layout   = createPipelineLayout(device, layoutBuilder, descPool);

        VkShaderModule vs = useGammaCorrection ? renderer->getShaderModule("gamma-correct-vert") : renderer->getShaderModule("fullscreen-quad-vert");
        VkShaderModule fs = useGammaCorrection ? renderer->getShaderModule("gamma-correct-frag") : renderer->getShaderModule("fullscreen-quad-frag");

        VkPipeline pipeline = PipelineBuilder()
            .setShaderStages
            ({
                createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT,   vs),
                createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fs)
            })
            .setFullScreenVertexLayout()
            .setViewport(renderer->getDefaultViewport())
            .setScissor(renderer->getDefaultScissor())
            .addDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
            .addDynamicState(VK_DYNAMIC_STATE_SCISSOR)
            .setBlendState(0, VK_TRUE)
            .setBlendFactors(0, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
            .setDepthWrite(VK_FALSE)
            .setDepthTest(VK_FALSE)
            .create(device->getHandle(), layout->getHandle(), renderPass->getHandle(), 0);

        return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout));
    }
}