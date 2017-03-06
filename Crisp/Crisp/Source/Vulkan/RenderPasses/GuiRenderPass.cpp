#include "GuiRenderPass.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    GuiRenderPass::GuiRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_renderTargetViews(VulkanRenderer::NumVirtualFrames)
        , m_depthTargetViews(VulkanRenderer::NumVirtualFrames)
        , m_framebuffers(VulkanRenderer::NumVirtualFrames)
        , m_clearValues(2, {})
        , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
    {
        m_clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };

        createRenderPass();
        createResources();
    }

    GuiRenderPass::~GuiRenderPass()
    {
        freeResources();
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    }

    void GuiRenderPass::begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer) const
    {
        // Transition the destination image layer into attachment layout
        VkImageMemoryBarrier transBarrier = {};
        transBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        transBarrier.oldLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transBarrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        transBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        transBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        transBarrier.srcAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
        transBarrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        transBarrier.image                           = m_renderTarget;
        transBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        transBarrier.subresourceRange.baseMipLevel   = 0;
        transBarrier.subresourceRange.levelCount     = 1;
        transBarrier.subresourceRange.baseArrayLayer = m_renderer->getCurrentFrameIndex();
        transBarrier.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &transBarrier);

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage GuiRenderPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTarget;
    }

    VkFormat GuiRenderPass::getColorFormat() const
    {
        return m_colorFormat;
    }

    void GuiRenderPass::createRenderPass()
    {
        // Description for color attachment
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format         = m_colorFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Description for depth attachment
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format         = m_depthFormat;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subPass = {};
        subPass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subPass.colorAttachmentCount    = 1;
        subPass.pColorAttachments       = &colorAttachmentRef;
        subPass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subPass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);
    }

    void GuiRenderPass::createResources()
    {
        m_renderTarget = m_renderer->getDevice().createDeviceImageArray(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height,
            VulkanRenderer::NumVirtualFrames, m_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
        m_renderer->getDevice().transitionImageLayout(m_renderTarget, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanRenderer::NumVirtualFrames);

        m_depthTarget = m_renderer->getDevice().createDeviceImageArray(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height,
            VulkanRenderer::NumVirtualFrames, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
        m_renderer->getDevice().transitionImageLayout(m_depthTarget, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VulkanRenderer::NumVirtualFrames);

        m_renderTargetViews.resize(VulkanRenderer::NumVirtualFrames);
        m_depthTargetViews.resize(VulkanRenderer::NumVirtualFrames);
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[i] = m_renderer->getDevice().createImageView(m_renderTarget, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
            m_depthTargetViews[i] = m_renderer->getDevice().createImageView(m_depthTarget, VK_IMAGE_VIEW_TYPE_2D, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);
            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[i],
                m_depthTargetViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = m_renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
            framebufferInfo.pAttachments    = attachmentViews.data();
            framebufferInfo.width           = m_renderer->getSwapChainExtent().width;
            framebufferInfo.height          = m_renderer->getSwapChainExtent().height;
            framebufferInfo.layers          = 1;
            vkCreateFramebuffer(m_renderer->getDevice().getHandle(), &framebufferInfo, nullptr, &m_framebuffers[i]);
        }
    }

    void GuiRenderPass::freeResources()
    {
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
            vkDestroyImageView(m_device, m_renderTargetViews[i], nullptr);
            vkDestroyImageView(m_device, m_depthTargetViews[i], nullptr);
            m_framebuffers[i]      = VK_NULL_HANDLE;
            m_renderTargetViews[i] = VK_NULL_HANDLE;
            m_depthTargetViews[i]  = VK_NULL_HANDLE;
        }

        m_renderer->getDevice().destroyDeviceImage(m_renderTarget);
        m_renderer->getDevice().destroyDeviceImage(m_depthTarget);

        m_renderTarget = VK_NULL_HANDLE;
        m_depthTarget  = VK_NULL_HANDLE;
    }
}