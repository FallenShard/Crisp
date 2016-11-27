#include "GuiRenderPass.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    GuiRenderPass::GuiRenderPass(VulkanRenderer* renderer)
        : m_renderer(renderer)
        , m_renderPass(VK_NULL_HANDLE)
        , m_device(renderer->getDevice().getHandle())
        , m_renderTargetViews(VulkanRenderer::NumVirtualFrames)
        , m_depthTargetViews(VulkanRenderer::NumVirtualFrames)
        , m_framebuffers(VulkanRenderer::NumVirtualFrames)
        , m_colorFormat(VK_FORMAT_B8G8R8A8_UINT)
        , m_depthStencilFormat(VK_FORMAT_D32_SFLOAT)
    {
        create();
    }

    GuiRenderPass::~GuiRenderPass()
    {
        freeResources();
    }

    void GuiRenderPass::recreate()
    {
        freeResources();
        create();
    }

    VkRenderPass GuiRenderPass::getHandle() const
    {
        return m_renderPass;
    }

    void GuiRenderPass::create()
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

        //// Description for depth attachment
        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format         = m_depthStencilFormat;
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
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subPass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);

        m_renderTarget = m_renderer->getDevice().createDeviceImageArray(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height,
            VulkanRenderer::NumVirtualFrames, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        m_depthTarget = m_renderer->getDevice().createDeviceImageArray(m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height,
            VulkanRenderer::NumVirtualFrames, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        m_renderTargetViews.resize(VulkanRenderer::NumVirtualFrames);
        m_depthTargetViews.resize(VulkanRenderer::NumVirtualFrames);
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[i] = m_renderer->getDevice().createImageView(m_renderTarget, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
            m_depthTargetViews[i] = m_renderer->getDevice().createImageView(m_depthTarget, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, i, 1);
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
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderer->getDevice().destroyDeviceImage(m_renderTarget);
        m_renderer->getDevice().destroyDeviceImage(m_depthTarget);

        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            vkDestroyImageView(m_device, m_renderTargetViews[i], nullptr);
            vkDestroyImageView(m_device, m_depthTargetViews[i], nullptr);
            vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
        }
    }
}