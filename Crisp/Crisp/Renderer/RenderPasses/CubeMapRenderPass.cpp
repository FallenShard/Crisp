#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRenderPassBuilder.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, const VkExtent2D renderArea, const VkFormat format) {
    VulkanRenderPassBuilder builder;
    builder.setAttachmentCount(1)
        .setAttachmentFormat(0, format)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setAttachmentClearColor(0, {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}})
        .setSubpassCount(1)
        .addColorAttachmentRef(0, 0)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, kExternalColorSubpass >> kColorWrite);
    return builder.create(device, renderArea);
}
} // namespace crisp