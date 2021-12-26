#include "BlurPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{

std::unique_ptr<VulkanRenderPass> createBlurPass(Renderer& renderer, VkFormat format,
    std::optional<VkExtent2D> renderArea)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(format, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = !renderArea.has_value();
    description.renderArea = renderArea;
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

} // namespace crisp