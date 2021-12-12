#include "DefaultRenderPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

namespace crisp
{
    DefaultRenderPass::DefaultRenderPass(Renderer* renderer)
        : VulkanRenderPass(renderer, true, 1)
    {
        m_clearValues.resize(1);
        m_clearValues[0].color = { 0.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(m_renderer->getSwapChain().getImageFormat(), VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                       .create(m_renderer->getDevice().getHandle());
        m_finalLayouts = { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };

        createResources();
    }

    void DefaultRenderPass::recreateFramebuffer(VkImageView swapChainImageView)
    {
        const uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
        if (m_framebuffers[frameIdx] && m_framebuffers[frameIdx]->getAttachment(0) == swapChainImageView)
            return;

        if (!m_imageFramebuffers.contains(swapChainImageView))
        {
            const auto attachmentViews = { swapChainImageView };
            m_imageFramebuffers.emplace(swapChainImageView, std::make_unique<VulkanFramebuffer>(m_renderer->getDevice(), m_handle, m_renderArea, attachmentViews));
        }

        if (m_framebuffers[frameIdx])
            m_framebuffers[frameIdx].swap(m_imageFramebuffers.at(m_framebuffers[frameIdx]->getAttachment(0)));

        m_framebuffers[frameIdx].swap(m_imageFramebuffers.at(swapChainImageView));
    }

    void DefaultRenderPass::createResources()
    {
        m_renderArea = m_renderer->getSwapChainExtent();
        m_framebuffers.resize(RendererConfig::VirtualFrameCount);
        m_imageFramebuffers.clear();
    }
}