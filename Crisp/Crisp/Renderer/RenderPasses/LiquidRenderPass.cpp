#include <Crisp/Renderer/RenderPasses/LiquidRenderPass.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>


namespace crisp {
std::unique_ptr<VulkanRenderPass> createLiquidRenderPass(const VulkanDevice& device, VkExtent2D renderArea) {
    return RenderPassBuilder()
        /*.setRenderTargetCount(3)
        .setRenderTargetFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        .setRenderTargetFormat(1, VK_FORMAT_D32_SFLOAT)
        .configureColorRenderTarget(1, 0)
        .setRenderTargetFormat(2, VK_FORMAT_R32G32B32A32_SFLOAT)
        .configureColorRenderTarget(2, VK_IMAGE_USAGE_SAMPLED_BIT)*/

        .setAttachmentCount(3)
        //.setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        //.setAttachmentMapping(1, 1)
        .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
        .setAttachmentLayouts(
            1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        //.setAttachmentMapping(2, 2)
        .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(2)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addInputAttachmentRef(1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .addColorAttachmentRef(1, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .addDependency(
            0,
            1,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
        .create(device, renderArea, {});
}
} // namespace crisp