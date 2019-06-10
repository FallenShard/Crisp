#include "FullScreenQuadPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanRenderPass.hpp"
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
            });

        VulkanDevice* device = renderer->getDevice();

        std::vector<bool> setBuffered = { true };
        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { Renderer::NumVirtualFrames }, Renderer::NumVirtualFrames);
        auto layout   = createPipelineLayout(device, layoutBuilder, setBuffered, descPool);

        VkShaderModule vs = useGammaCorrection ? renderer->getShaderModule("gamma-correct-vert") : renderer->getShaderModule("fullscreen-quad-vert");
        VkShaderModule fs = useGammaCorrection ? renderer->getShaderModule("gamma-correct-frag") : renderer->getShaderModule("fullscreen-quad-frag");

        return PipelineBuilder()
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
            .create(device, std::move(layout), renderPass->getHandle(), 0);
    }
}