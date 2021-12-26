#include "DefaultRenderPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

namespace crisp
{
DefaultRenderPass::DefaultRenderPass(Renderer& renderer)
    : VulkanRenderPass(renderer, true, 1)
{
    std::tie(m_handle, m_attachmentDescriptions) =
        RenderPassBuilder()
            .addAttachment(renderer.getSwapChain().getImageFormat(), VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());
    m_renderTargetInfos.resize(m_attachmentDescriptions.size());
    m_attachmentClearValues.resize(m_attachmentDescriptions.size());
    m_attachmentClearValues[0].color = { 0.0f };

    createResources(renderer);
}

void DefaultRenderPass::recreateFramebuffer(Renderer& renderer, VkImageView swapChainImageView)
{
    const uint32_t frameIdx = renderer.getCurrentVirtualFrameIndex();
    if (m_framebuffers[frameIdx] && m_framebuffers[frameIdx]->getAttachment(0) == swapChainImageView)
        return;

    if (!m_imageFramebuffers.contains(swapChainImageView))
    {
        const auto attachmentViews = { swapChainImageView };
        m_imageFramebuffers.emplace(swapChainImageView,
            std::make_unique<VulkanFramebuffer>(renderer.getDevice(), m_handle, m_renderArea, attachmentViews));
    }

    if (m_framebuffers[frameIdx])
        m_framebuffers[frameIdx].swap(m_imageFramebuffers.at(m_framebuffers[frameIdx]->getAttachment(0)));

    m_framebuffers[frameIdx].swap(m_imageFramebuffers.at(swapChainImageView));
}

void DefaultRenderPass::createResources(Renderer& renderer)
{
    m_renderArea = renderer.getSwapChainExtent();
    m_framebuffers.resize(RendererConfig::VirtualFrameCount);
    m_imageFramebuffers.clear();
}
} // namespace crisp