#include "ShadowPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <Crisp/Enumerate.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createShadowMapPass(
    const VulkanDevice& device, uint32_t shadowMapSize, uint32_t cascadeCount)
{
    RenderPassBuilder builder;
    builder.setSwapChainDependency(false)
        .setRenderTargetsBuffered(true)

        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_D32_SFLOAT)
        .setRenderTargetLayersAndMipmaps(0, cascadeCount)
        .configureDepthRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT, {1.0f, 0})

        .setNumSubpasses(cascadeCount)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    builder.setAttachmentCount(cascadeCount);
    for (uint32_t i = 0; i < cascadeCount; ++i)
    {
        builder.setAttachmentMapping(i, 0, i, 1)
            .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(
                i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .setDepthAttachmentRef(i, i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    return builder.create(device, {shadowMapSize, shadowMapSize});
}
} // namespace crisp