#include <Crisp/Renderer/RenderPasses/BlurPass.hpp>

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRenderPassBuilder.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createBlurPass(
    const VulkanDevice& device,
    RenderTargetCache& renderTargetCache,
    const VkFormat format,
    const VkExtent2D renderArea,
    const bool isSwapChainDependent,
    std::string&& renderTargetName) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        std::move(renderTargetName),
        RenderTargetBuilder()
            .setFormat(format)
            .setLayerAndMipLevelCount(1)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT)
            .setSize(renderArea, isSwapChainDependent)
            .create(device));

    return VulkanRenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setSubpassCount(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea, renderTargets);
}

} // namespace crisp