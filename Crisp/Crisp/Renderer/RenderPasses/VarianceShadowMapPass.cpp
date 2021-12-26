#include "VarianceShadowMapPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createVarianceShadowMappingPass(Renderer& renderer, unsigned int shadowMapSize)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_R32G32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = false;
    description.renderArea = { shadowMapSize, shadowMapSize };
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT);
    description.renderTargetInfos[1].configureDepthRenderTarget(0);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}
} // namespace crisp