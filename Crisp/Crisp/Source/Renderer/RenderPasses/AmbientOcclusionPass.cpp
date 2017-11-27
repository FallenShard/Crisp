#include "AmbientOcclusionPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"

namespace crisp
{
    AmbientOcclusionPass::AmbientOcclusionPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_clearValues(1)
        , m_colorFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
    {
        m_clearValues[0].color = { 0.0f, 1.0f, 0.0f, 1.0f };

        createRenderPass();
        createResources();
    }

    AmbientOcclusionPass::~AmbientOcclusionPass()
    {
        freeResources();
    }

    void AmbientOcclusionPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderer->getSwapChainExtent();
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage AmbientOcclusionPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTargets[index]->getImage()->getHandle();
    }

    VkImageView AmbientOcclusionPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return m_renderTargetViews.at(index).at(frameIndex)->getHandle();
    }

    VkFormat AmbientOcclusionPass::getColorFormat() const
    {
        return m_colorFormat;
    }

    VkFramebuffer AmbientOcclusionPass::getFramebuffer(unsigned int index) const
    {
        return m_framebuffers[index];
    }

    std::unique_ptr<TextureView> AmbientOcclusionPass::createRenderTargetView(unsigned int index) const
    {
        return m_renderTargets[index]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, VulkanRenderer::NumVirtualFrames);
    }

    void AmbientOcclusionPass::createRenderPass()
    {
        VkAttachmentDescription attachment = {};
        attachment.format         = m_colorFormat;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription opaqueSubpass = {};
        opaqueSubpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        opaqueSubpass.colorAttachmentCount    = 1;
        opaqueSubpass.pColorAttachments       = &colorRef;

        std::vector<VkSubpassDescription> subpasses = { opaqueSubpass };

        VkSubpassDependency dependency = {};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        std::vector<VkSubpassDependency> deps = { dependency };

        VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &attachment;
        renderPassInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses      = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(deps.size());
        renderPassInfo.pDependencies   = deps.data();

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void AmbientOcclusionPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderer->getSwapChainExtent().width, m_renderer->getSwapChainExtent().height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        m_renderTargets[0] = std::make_shared<Texture>(m_renderer, extent, numLayers, m_colorFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(1, std::vector<std::shared_ptr<TextureView>>(numLayers));
        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[0][i]->getHandle()
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

    void AmbientOcclusionPass::freeResources()
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