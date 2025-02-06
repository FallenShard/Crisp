#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, const VkExtent2D renderArea, const VkFormat format) {
    // RenderPassBuilder builder;
    // builder.setAttachmentCount(6).setSubpassCount(6).addDependency(
    //     VK_SUBPASS_EXTERNAL,
    //     0,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_READ_BIT,
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    //     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    // RenderPassCreationParams creationParams{};
    // for (int i = 0; i < 6; i++) {
    //     builder.setAttachmentFormat(i, VK_FORMAT_R8G8B8A8_UNORM)
    //         .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
    //         .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) .addColorAttachmentRef(i, i,
    //         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //     creationParams.clearValues.push_back({.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    // }
    RenderPassBuilder builder;
    builder.setAttachmentCount(1)
        .setAttachmentFormat(0, format)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setSubpassCount(1)
        .addColorAttachmentRef(0, 0)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, kExternalColorSubpass >> kColorWrite);

    RenderPassCreationParams creationParams{};
    creationParams.clearValues.push_back({.color = {{0.0f, 0.0f, 0.0f, 1.0f}}});
    return builder.create(device, renderArea, creationParams);
}
} // namespace crisp