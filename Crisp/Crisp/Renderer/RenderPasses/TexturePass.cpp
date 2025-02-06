#include <Crisp/Renderer/RenderPasses/TexturePass.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createTexturePass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea, VkFormat textureFormat) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "TexturePass",
        RenderTargetBuilder()
            .setFormat(textureFormat)
            .setLayerAndMipLevelCount(1)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize(renderArea, false)
            .create(device));

    return RenderPassBuilder()
        .setAttachmentCount(1)
        // .setAttachmentMapping(0, 0)
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
        .create(device, renderArea, {});
}

std::unique_ptr<VulkanRenderPass> createTexturePass(
    const VulkanDevice& device, const VkExtent2D renderArea, const VkFormat textureFormat) {

    RenderPassCreationParams creationParams{};
    creationParams.clearValues.push_back({.color = {{0, 0, 0, 1}}});

    return RenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentFormat(0, textureFormat)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setSubpassCount(1)
        .addColorAttachmentRef(0, 0)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, kExternalColorSubpass >> kColorWrite)
        .create(device, renderArea, creationParams);
}

} // namespace crisp