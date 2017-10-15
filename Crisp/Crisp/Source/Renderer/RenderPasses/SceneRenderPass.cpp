#include "SceneRenderPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"

namespace crisp
{
    SceneRenderPass::SceneRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_clearValues(RenderTarget::Count)
        , m_colorFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
    {
        m_clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };

        createRenderPass();
        createResources();
    }

    SceneRenderPass::~SceneRenderPass()
    {
        freeResources();
    }

    void SceneRenderPass::begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage SceneRenderPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTargets[index]->getImage()->getHandle();
    }

    VkImageView SceneRenderPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return m_renderTargetViews.at(index).at(frameIndex)->getHandle();
    }

    VkFormat SceneRenderPass::getColorFormat() const
    {
        return m_colorFormat;
    }

    VkFramebuffer SceneRenderPass::getFramebuffer(unsigned int index) const
    {
        return m_framebuffers[index];
    }

    std::unique_ptr<TextureView> SceneRenderPass::createRenderTargetView(unsigned int index) const
    {
        return m_renderTargets[index]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, VulkanRenderer::NumVirtualFrames);
    }

    void SceneRenderPass::createRenderPass()
    {
        std::vector<VkAttachmentDescription> attachments(RenderTarget::Count, VkAttachmentDescription{});
        attachments[Opaque].format         = m_colorFormat;
        attachments[Opaque].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[Opaque].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[Opaque].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[Opaque].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[Opaque].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Opaque].initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[Opaque].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        attachments[Depth].format         = m_depthFormat;
        attachments[Depth].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[Depth].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[Depth].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[Depth].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[Depth].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Depth].initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[Depth].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorRef = { RenderTarget::Opaque, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef = { RenderTarget::Depth,  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription opaqueSubpass = {};
        opaqueSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        opaqueSubpass.colorAttachmentCount    = 1;
        opaqueSubpass.pColorAttachments       = &colorRef;
        opaqueSubpass.pDepthStencilAttachment = &depthRef;

        std::vector<VkSubpassDescription> subpasses =
        {
            opaqueSubpass
        };

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::vector<VkSubpassDependency> deps = { dependency };

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses      = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(deps.size());
        renderPassInfo.pDependencies   = deps.data();

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void SceneRenderPass::createResources()
    {
        m_renderTargets.resize(RenderTarget::Count);
        VkExtent3D extent = { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        m_renderTargets[Opaque] = std::make_shared<Texture>(m_renderer, extent, numLayers, m_colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_renderTargets[Depth] = std::make_shared<Texture>(m_renderer, extent, numLayers, m_depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[Opaque]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
            m_renderTargets[Depth]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
        });

        m_renderTargetViews.resize(RenderTarget::Count, std::vector<std::shared_ptr<TextureView>>(VulkanRenderer::NumVirtualFrames));
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            m_renderTargetViews[Opaque][i] = m_renderTargets[Opaque]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[Depth ][i] = m_renderTargets[Depth ]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[Opaque][i]->getHandle(),
                m_renderTargetViews[Depth ][i]->getHandle(),
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

    void SceneRenderPass::freeResources()
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