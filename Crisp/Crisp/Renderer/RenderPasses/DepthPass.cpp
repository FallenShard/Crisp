#include "DepthPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanFramebuffer.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createDepthPass(Renderer& renderer)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .setDepthAttachmentRef(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = true;
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureDepthRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT);
    description.renderTargetInfos[0].initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

} // namespace crisp