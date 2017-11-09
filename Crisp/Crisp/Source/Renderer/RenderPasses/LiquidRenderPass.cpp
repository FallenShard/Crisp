#include "LiquidRenderPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace crisp
{
    LiquidRenderPass::LiquidRenderPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_clearValues(RenderTarget::Count)
        , m_colorFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
    {
        m_clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };
        m_clearValues[2].color        = { 0.0f, 0.0f, 0.0f, 0.0f };

        createRenderPass();
        createResources();
    }

    LiquidRenderPass::~LiquidRenderPass()
    {
        freeResources();
    }

    void LiquidRenderPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()]->getHandle();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage LiquidRenderPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTargets[index]->getImage()->getHandle();
    }

    VkImageView LiquidRenderPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return m_renderTargetViews.at(index).at(frameIndex)->getHandle();
    }

    VkFormat LiquidRenderPass::getColorFormat() const
    {
        return m_colorFormat;
    }

    VkFramebuffer LiquidRenderPass::getFramebuffer(unsigned int index) const
    {
        return m_framebuffers[index]->getHandle();
    }

    std::unique_ptr<TextureView> LiquidRenderPass::createRenderTargetView(unsigned int index) const
    {
        return m_renderTargets[index]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, VulkanRenderer::NumVirtualFrames);
    }

    void LiquidRenderPass::createRenderPass()
    {
        std::vector<VkAttachmentDescription> attachments(RenderTarget::Count, VkAttachmentDescription{});
        attachments[GBuffer].format         = m_colorFormat;
        attachments[GBuffer].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[GBuffer].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[GBuffer].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[GBuffer].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[GBuffer].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[GBuffer].initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[GBuffer].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        attachments[Depth].format         = m_depthFormat;
        attachments[Depth].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[Depth].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[Depth].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Depth].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[Depth].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[Depth].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[Depth].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments[LiquidMask].format         = m_colorFormat;
        attachments[LiquidMask].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[LiquidMask].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[LiquidMask].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[LiquidMask].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[LiquidMask].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[LiquidMask].initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[LiquidMask].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference gBufferRef = { RenderTarget::GBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef =   { RenderTarget::Depth,   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        
        VkSubpassDescription geometrySubpass = {};
        geometrySubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        geometrySubpass.colorAttachmentCount    = 1;
        geometrySubpass.pColorAttachments       = &gBufferRef;
        geometrySubpass.pDepthStencilAttachment = &depthRef;

        VkAttachmentReference liquidMaskRef   = { RenderTarget::LiquidMask, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference gBufferInputRef = { RenderTarget::GBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        VkSubpassDescription shadingSubpass = {};
        shadingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        shadingSubpass.colorAttachmentCount = 1;
        shadingSubpass.pColorAttachments    = &liquidMaskRef;
        shadingSubpass.inputAttachmentCount = 1;
        shadingSubpass.pInputAttachments    = &gBufferInputRef;

        std::vector<VkSubpassDescription> subpasses =
        {
            geometrySubpass,
            shadingSubpass
        };

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkSubpassDependency gBufferDep = {};
        gBufferDep.srcSubpass = 0;
        gBufferDep.dstSubpass = 1;
        gBufferDep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        gBufferDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        gBufferDep.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        gBufferDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;

        std::vector<VkSubpassDependency> deps = { dependency, gBufferDep };

        VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses      = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(deps.size());
        renderPassInfo.pDependencies   = deps.data();

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void LiquidRenderPass::createResources()
    {
        m_renderTargets.resize(RenderTarget::Count);
        VkExtent3D extent = { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        m_renderTargets[GBuffer] = std::make_unique<Texture>(m_renderer, extent, numLayers, m_colorFormat,
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        m_renderTargets[Depth]   = std::make_unique<Texture>(m_renderer, extent, numLayers, m_depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
        m_renderTargets[LiquidMask] = std::make_unique<Texture>(m_renderer, extent, numLayers, m_colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[GBuffer]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
            m_renderTargets[Depth]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
            m_renderTargets[LiquidMask]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
        });

        m_renderTargetViews.resize(RenderTarget::Count);
        for (auto& views : m_renderTargetViews)
            views.resize(VulkanRenderer::NumVirtualFrames);

        m_framebuffers.reserve(VulkanRenderer::NumVirtualFrames);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            m_renderTargetViews[GBuffer][i]    = m_renderTargets[GBuffer]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[Depth][i]      = m_renderTargets[Depth]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            m_renderTargetViews[LiquidMask][i] = m_renderTargets[LiquidMask]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[GBuffer][i]->getHandle(),
                m_renderTargetViews[Depth][i]->getHandle(),
                m_renderTargetViews[LiquidMask][i]->getHandle()
            };

            m_framebuffers.emplace_back(std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderer->getSwapChainExtent(), attachmentViews));
        }
    }

    void LiquidRenderPass::freeResources()
    {
        m_renderTargets.clear();
        m_renderTargetViews.clear();
        m_framebuffers.clear();
    }
}