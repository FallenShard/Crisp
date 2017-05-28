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
        , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
    {
        m_clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };
        m_clearValues[2].color        = { 0.0f, 0.0f, 0.0f, 0.0f };

        createRenderPass();
        createResources();
    }

    SceneRenderPass::~SceneRenderPass()
    {
        vkDestroyRenderPass(m_device->getHandle(), m_renderPass, nullptr);
        freeResources();
    }

    void SceneRenderPass::begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer) const
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
        transBarrier.image                           = m_renderTargets[RenderTarget::Composited]->getImage()->getHandle();
        transBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        transBarrier.subresourceRange.baseMipLevel   = 0;
        transBarrier.subresourceRange.levelCount     = 1;
        transBarrier.subresourceRange.baseArrayLayer = m_renderer->getCurrentVirtualFrameIndex();
        transBarrier.subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &transBarrier);

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();;
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
        attachments[Opaque].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[Opaque].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachments[Depth].format         = m_depthFormat;
        attachments[Depth].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[Depth].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[Depth].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Depth].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[Depth].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Depth].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[Depth].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments[Composited].format         = VK_FORMAT_R32G32B32A32_SFLOAT;
        attachments[Composited].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[Composited].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[Composited].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[Composited].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[Composited].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Composited].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[Composited].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference opaqueSubpassColorAttachmentRef = {};
        opaqueSubpassColorAttachmentRef.attachment = RenderTarget::Opaque;
        opaqueSubpassColorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference opaqueSubpassDepthAttachmentRef = {};
        opaqueSubpassDepthAttachmentRef.attachment = RenderTarget::Depth;
        opaqueSubpassDepthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription opaqueSubpass = {};
        opaqueSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        opaqueSubpass.colorAttachmentCount    = 1;
        opaqueSubpass.pColorAttachments       = &opaqueSubpassColorAttachmentRef;
        opaqueSubpass.pDepthStencilAttachment = &opaqueSubpassDepthAttachmentRef;

        std::vector<VkAttachmentReference> compositeRefs =
        {
            { RenderTarget::Composited, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
        };

        std::vector<VkAttachmentReference> compositeInputRefs =
        {
            { RenderTarget::Opaque, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
        };

        VkSubpassDescription compositeSubpass = {};
        compositeSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        //compositeSubpass.inputAttachmentCount    = static_cast<uint32_t>(compositeInputRefs.size());
        //compositeSubpass.pInputAttachments       = compositeInputRefs.data();
        compositeSubpass.colorAttachmentCount    = static_cast<uint32_t>(compositeRefs.size());
        compositeSubpass.pColorAttachments       = compositeRefs.data();
        compositeSubpass.pDepthStencilAttachment = &opaqueSubpassDepthAttachmentRef;

        std::vector<VkSubpassDescription> subpasses =
        {
            opaqueSubpass,
            compositeSubpass
        };

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkSubpassDependency opaqueToCompositeDependency = {};
        opaqueToCompositeDependency.srcSubpass    = 0;
        opaqueToCompositeDependency.dstSubpass    = 1;
        opaqueToCompositeDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        opaqueToCompositeDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        opaqueToCompositeDependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        opaqueToCompositeDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::vector<VkSubpassDependency> deps = { dependency, opaqueToCompositeDependency };

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses      = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(deps.size());
        renderPassInfo.pDependencies   = deps.data();

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_renderPass);
    }

    void SceneRenderPass::createResources()
    {
        m_renderTargets.resize(RenderTarget::Count);
        VkExtent3D extent = { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        m_renderTargets[Opaque] = std::make_shared<Texture>(m_renderer, extent, numLayers, m_colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_renderTargets[Depth] = std::make_shared<Texture>(m_renderer, extent, numLayers, m_depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        m_renderTargets[Composited] = std::make_shared<Texture>(m_renderer, extent, numLayers, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[Opaque]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
            m_renderTargets[Depth]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
            m_renderTargets[Composited]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
        });

        m_renderTargetViews.resize(RenderTarget::Count, std::vector<std::shared_ptr<TextureView>>(VulkanRenderer::NumVirtualFrames));
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            m_renderTargetViews[Opaque    ][i] = m_renderTargets[Opaque    ]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[Depth     ][i] = m_renderTargets[Depth     ]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[Composited][i] = m_renderTargets[Composited]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[Opaque    ][i]->getHandle(),
                m_renderTargetViews[Depth     ][i]->getHandle(),
                m_renderTargetViews[Composited][i]->getHandle()
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = m_renderPass;
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