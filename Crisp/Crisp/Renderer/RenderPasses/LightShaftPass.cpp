#include <Crisp/Renderer/RenderPasses/LightShaftPass.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp {
std::unique_ptr<VulkanRenderPass> createLightShaftPass(const VulkanDevice& device, VkExtent2D renderArea) {
    return RenderPassBuilder()
        /*.setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R8G8B8A8_UNORM)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT)*/

        .setAttachmentCount(1)
        //.setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
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
} // namespace crisp