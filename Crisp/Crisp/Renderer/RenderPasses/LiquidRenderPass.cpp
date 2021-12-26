#include "LiquidRenderPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{
LiquidRenderPass::LiquidRenderPass(Renderer& renderer)
    : VulkanRenderPass(renderer, true, 2)
{
    RenderPassBuilder builder{};

    std::tie(m_handle, m_attachmentDescriptions) =
        builder.addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(GBuffer, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(GBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(Depth, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .setAttachmentLayouts(Depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(LiquidMask, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(LiquidMask, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(2)
            .addColorAttachmentRef(0, GBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setDepthAttachmentRef(0, Depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(1, LiquidMask, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addInputAttachmentRef(1, GBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .addDependency(0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
            .create(renderer.getDevice().getHandle());
    m_renderTargetInfos.resize(m_attachmentDescriptions.size());
    setColorRenderTargetInfo(0, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    setDepthRenderTargetInfo(1, 0);
    setColorRenderTargetInfo(2, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

    createResources(renderer);
}
} // namespace crisp