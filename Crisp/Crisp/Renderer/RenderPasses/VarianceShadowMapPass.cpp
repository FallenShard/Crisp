#include "VarianceShadowMapPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(const VulkanDevice& device,
    unsigned int shadowMapSize)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(false)
        .setRenderTargetCount(2)
        .setRenderTargetFormat(0, VK_FORMAT_R32G32_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT)
        .setRenderTargetFormat(1, VK_FORMAT_D32_SFLOAT)
        .configureDepthRenderTarget(1, 0)

        .setAttachmentCount(2)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setAttachmentMapping(1, 1)
        .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
        .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, { shadowMapSize, shadowMapSize });
}
} // namespace crisp