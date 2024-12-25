#include <Crisp/Renderer/RenderPasses/CubeMapRenderPass.hpp>

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

namespace crisp {

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, RenderTarget* renderTarget, VkExtent2D renderArea, uint32_t mipLevelIndex) {
    RenderPassBuilder builder;
    builder.setAttachmentCount(6).setNumSubpasses(6).addDependency(
        VK_SUBPASS_EXTERNAL,
        0,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    for (int i = 0; i < 6; i++) {
        builder.setAttachmentMapping(i, 0, i, 1, mipLevelIndex, 1)
            .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(i, i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    return builder.create(device, renderArea, {renderTarget});
}
} // namespace crisp