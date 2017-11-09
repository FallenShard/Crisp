#include "DefaultRenderPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanSwapChain.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    DefaultRenderPass::DefaultRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_colorFormat(m_renderer->getSwapChain()->getImageFormat())
        , m_framebuffers(VulkanRenderer::NumVirtualFrames)
    {
        m_clearValue.color = { 0.1f, 0.1f, 0.1f, 1.0f };
        createRenderPass();
        createResources();
    }

    DefaultRenderPass::~DefaultRenderPass()
    {
        freeResources();

        for (auto& framebuffer : m_framebuffers)
            if (framebuffer != VK_NULL_HANDLE)
                vkDestroyFramebuffer(m_device->getHandle(), framebuffer, nullptr);
    }

    void DefaultRenderPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();
        renderPassInfo.clearValueCount   = 1;
        renderPassInfo.pClearValues      = &m_clearValue;

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage DefaultRenderPass::getColorAttachment(unsigned int index) const
    {
        return VK_NULL_HANDLE;
    }

    VkImageView DefaultRenderPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return VK_NULL_HANDLE;
    }

    void DefaultRenderPass::recreateFramebuffer(VkImageView swapChainImageView)
    {
        const uint32_t frameIdx = m_renderer->getCurrentVirtualFrameIndex();
        if (m_framebuffers[frameIdx] != VK_NULL_HANDLE)
            vkDestroyFramebuffer(m_device->getHandle(), m_framebuffers[frameIdx], nullptr);

        std::vector<VkImageView> attachmentViews = { swapChainImageView };

        VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferInfo.renderPass = m_handle;
        framebufferInfo.width      = m_renderer->getSwapChainExtent().width;
        framebufferInfo.height     = m_renderer->getSwapChainExtent().height;
        framebufferInfo.layers     = 1;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.pAttachments    = attachmentViews.data();
        
        vkCreateFramebuffer(m_device->getHandle(), &framebufferInfo, nullptr, &m_framebuffers[frameIdx]);
    }

    void DefaultRenderPass::createRenderPass()
    {
        // Description for color attachment
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format         = m_colorFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void DefaultRenderPass::createResources()
    {
        // Uses image resources given by swap chain
    }

    void DefaultRenderPass::freeResources()
    {
    }
}