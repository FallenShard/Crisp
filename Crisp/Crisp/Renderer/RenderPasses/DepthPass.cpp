#include <Crisp/Renderer/RenderPasses/DepthPass.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createDepthPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, const VkExtent2D renderArea) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "DepthPassMap",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_D32_SFLOAT)
            .setLayerAndMipLevelCount(1)
            .setBuffered(true)
            .configureDepthRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT, {1.0f, 0})
            .setSize(renderArea, true)
            .create(device));

    return RenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(DepthPassDepthAttachmentIndex, 0)
        .setAttachmentOps(DepthPassDepthAttachmentIndex, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(
            DepthPassDepthAttachmentIndex,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .setDepthAttachmentRef(0, DepthPassDepthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea, renderTargets);
}

} // namespace crisp