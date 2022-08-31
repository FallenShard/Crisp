#include "ShadowPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/RenderTargetCache.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <Crisp/Enumerate.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createShadowMapPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, uint32_t shadowMapSize, uint32_t cascadeIndex)
{
    const VkExtent2D renderArea{shadowMapSize, shadowMapSize};

    std::vector<RenderTarget*> renderTargets(1);
    if (cascadeIndex == 0)
    {
        renderTargets[0] = renderTargetCache.addRenderTarget(
            "ShadowMap",
            RenderTargetBuilder()
                .setFormat(VK_FORMAT_D32_SFLOAT)
                .setLayerAndMipLevelCount(4)
                .setBuffered(true)
                .configureDepthRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT, {1.0f, 0})
                .setSize({shadowMapSize, shadowMapSize})
                .create(device));
    }
    else
    {
        renderTargets[0] = renderTargetCache.get("ShadowMap");
    }

    return RenderPassBuilder()
        .setSwapChainDependency(false)
        .setRenderTargetsBuffered(true)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, renderTargets[0]->info, 0, cascadeIndex, 1)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(
            0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .setDepthAttachmentRef(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea, renderTargets);
}

std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, unsigned int shadowMapSize)
{
    const VkExtent2D renderArea{shadowMapSize, shadowMapSize};

    std::vector<RenderTarget*> renderTargets(2);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "ShadowMap",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R32G32_SFLOAT)
            .setBuffered(true)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT)
            .setSize(renderArea)
            .create(device));
    renderTargets[1] = renderTargetCache.addRenderTarget(
        "ShadowMap",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_D32_SFLOAT)
            .setBuffered(true)
            .configureDepthRenderTarget(0)
            .setSize(renderArea)
            .create(device));

    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(false)

        .setAttachmentCount(2)
        .setAttachmentMapping(0, renderTargets[0]->info, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setAttachmentMapping(1, renderTargets[1]->info, 1)
        .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
        .setAttachmentLayouts(
            1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea, renderTargets);
}

} // namespace crisp