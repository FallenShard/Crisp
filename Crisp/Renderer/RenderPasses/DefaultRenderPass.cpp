#include "DefaultRenderPass.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderPassBuilder.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"
#include "vulkan/VulkanSwapChain.hpp"

namespace crisp
{
    DefaultRenderPass::DefaultRenderPass(Renderer* renderer)
        : VulkanRenderPass(renderer, true, 1)
    {
        m_clearValues.resize(1);
        m_clearValues[0].color = { 0.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(m_renderer->getSwapChain()->getImageFormat(), VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());
        m_finalLayouts = { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };

        createResources();
    }

    void DefaultRenderPass::recreateFramebuffer(VkImageView swapChainImageView)
    {
        const uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();

        const auto attachmentViews = { swapChainImageView };
        m_framebuffers[frameIdx] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, attachmentViews);
    }

    void DefaultRenderPass::createResources()
    {
        m_renderArea = m_renderer->getSwapChainExtent();
        m_framebuffers.resize(Renderer::NumVirtualFrames);
    }
}