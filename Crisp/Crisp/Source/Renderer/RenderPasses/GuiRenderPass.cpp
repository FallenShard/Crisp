#include "GuiRenderPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"

namespace crisp
{
    GuiRenderPass::GuiRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
        , m_clearValues(2)
    {
        m_clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };

        createRenderPass();
        createResources();
    }

    GuiRenderPass::~GuiRenderPass()
    {
        freeResources();
    }

    void GuiRenderPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage GuiRenderPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTargets[0]->getImage()->getHandle();
    }

    VkImageView GuiRenderPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return m_renderTargetViews.at(index).at(frameIndex)->getHandle();
    }

    std::unique_ptr<TextureView> GuiRenderPass::createRenderTargetView(unsigned int index) const
    {
        return m_renderTargets[index]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, VulkanRenderer::NumVirtualFrames);
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
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
        dependency.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subPass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void GuiRenderPass::createResources()
    {
        m_renderTargets.resize(2);
        VkExtent3D extent = { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, 1u };
        m_renderTargets[0] = std::make_shared<Texture>(m_renderer, extent, VulkanRenderer::NumVirtualFrames, m_colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_renderTargets[1] = std::make_shared<Texture>(m_renderer, extent, VulkanRenderer::NumVirtualFrames, m_depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            m_renderTargets[1]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        });

        m_renderTargetViews.resize(2, std::vector<std::shared_ptr<TextureView>>(VulkanRenderer::NumVirtualFrames));
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[1][i] = m_renderTargets[1]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[0][i]->getHandle(),
                m_renderTargetViews[1][i]->getHandle()
            };
        
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = m_handle;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
            framebufferInfo.pAttachments    = attachmentViews.data();
            framebufferInfo.width           = m_renderer->getSwapChainExtent().width;
            framebufferInfo.height          = m_renderer->getSwapChainExtent().height;
            framebufferInfo.layers          = 1;
            vkCreateFramebuffer(m_device->getHandle(), &framebufferInfo, nullptr, &m_framebuffers[i]);
        }
    }

    void GuiRenderPass::freeResources()
    {
        m_renderTargets.clear();
        m_renderTargetViews.clear();
        for (int i = 0; i < VulkanRenderer::NumVirtualFrames; i++)
        {
            vkDestroyFramebuffer(m_device->getHandle(), m_framebuffers[i], nullptr);
            m_framebuffers[i] = VK_NULL_HANDLE;
        }
    }
}