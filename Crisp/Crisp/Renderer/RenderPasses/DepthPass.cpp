#include <Crisp/Renderer/RenderPasses/DepthPass.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanFramebuffer.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createDepthPass(const VulkanDevice& device, VkExtent2D renderArea)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(true)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_D32_SFLOAT)
        .configureDepthRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .setDepthAttachmentRef(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea);
}

} // namespace crisp