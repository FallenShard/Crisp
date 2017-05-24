#include "ShadowPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"

namespace crisp
{
    namespace
    {
        constexpr int shadowMapSize = 1024;
        constexpr int numCascades   = 3;
    }

    ShadowPass::ShadowPass(VulkanRenderer* renderer)
        : VulkanRenderPass(renderer)
        , m_clearValues(numCascades)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
    {
        m_clearValues[0].depthStencil = { 1.0f, 0 };
        m_clearValues[1].depthStencil = { 1.0f, 0 };
        m_clearValues[2].depthStencil = { 1.0f, 0 };
        m_renderArea = { shadowMapSize, shadowMapSize };

        createRenderPass();
        createResources();
    }

    ShadowPass::~ShadowPass()
    {
        vkDestroyRenderPass(m_device->getHandle(), m_renderPass, nullptr);
        freeResources();
    }

    void ShadowPass::begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderArea;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkImage ShadowPass::getColorAttachment(unsigned int index) const
    {
        return m_renderTargets[index]->getImage()->getHandle();
    }

    VkImageView ShadowPass::getAttachmentView(unsigned int index, unsigned int frameIndex) const
    {
        return m_renderTargetViews.at(index).at(frameIndex)->getHandle();
    }

    std::unique_ptr<TextureView> ShadowPass::createRenderTargetView(unsigned int index) const
    {
        return m_renderTargets[index]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, VulkanRenderer::NumVirtualFrames);
    }

    VkFramebuffer ShadowPass::getFramebuffer(unsigned int index) const
    {
        return m_framebuffers[index];
    }

    void ShadowPass::createRenderPass()
    {
        std::vector<VkAttachmentDescription> attachments(3, VkAttachmentDescription{});
        for (auto& attachment : attachments)
        {
            attachment.format         = m_depthFormat;
            attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        std::vector<VkAttachmentReference> depthAttachmentRefs = 
        {
            { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
            { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
            { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
        };

        std::vector<VkSubpassDescription> subpasses(numCascades);
        for (int i = 0; i < numCascades; i++)
        {
            subpasses[i].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount    = 0;
            subpasses[i].pColorAttachments       = nullptr;
            subpasses[i].pDepthStencilAttachment = &depthAttachmentRefs[i];
        }

        std::vector<VkSubpassDependency> dependencies(numCascades);
        for (int i = 0; i < numCascades; i++)
        {
            dependencies[i].srcSubpass    = VK_SUBPASS_EXTERNAL;
            dependencies[i].dstSubpass    = 0;
            dependencies[i].srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[i].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[i].dstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependencies[i].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = static_cast<uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses      = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies   = dependencies.data();

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_renderPass);
    }

    void ShadowPass::createResources()
    {
        m_renderTargets.resize(numCascades);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        for (auto& rt : m_renderTargets)
            rt = std::make_shared<Texture>(m_renderer, extent, numLayers, m_depthFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        m_renderer->addImageTransition([this](VkCommandBuffer& cmdBuffer)
        {
            for (auto& rt : m_renderTargets)
                rt->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
        });

        m_renderTargetViews.resize(numCascades, std::vector<std::shared_ptr<TextureView>>(VulkanRenderer::NumVirtualFrames));
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            for (uint32_t j = 0; j < numCascades; j++)
                m_renderTargetViews[j][i] = m_renderTargets[j]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            std::vector<VkImageView> attachmentViews =
            {
                m_renderTargetViews[0][i]->getHandle(),
                m_renderTargetViews[1][i]->getHandle(),
                m_renderTargetViews[2][i]->getHandle()
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = m_renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
            framebufferInfo.pAttachments    = attachmentViews.data();
            framebufferInfo.width           = m_renderArea.width;
            framebufferInfo.height          = m_renderArea.height;
            framebufferInfo.layers          = 1;
            vkCreateFramebuffer(m_device->getHandle(), &framebufferInfo, nullptr, &m_framebuffers[i]);
        }
    }

    void ShadowPass::freeResources()
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