#include "ShadowPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"

#include "Renderer/Texture.hpp"
#include "Renderer/TextureView.hpp"

namespace crisp
{
    ShadowPass::ShadowPass(VulkanRenderer* renderer, unsigned int shadowMapSize, unsigned int numCascades)
        : VulkanRenderPass(renderer)
        , m_clearValues(numCascades)
        , m_depthFormat(VK_FORMAT_D32_SFLOAT)
        , m_numCascades(numCascades)
    {
        for (auto& clearValue : m_clearValues)
            clearValue.depthStencil = { 1.0f, 0 };

        m_renderArea = { shadowMapSize, shadowMapSize };

        createRenderPass();
        createResources();
    }

    ShadowPass::~ShadowPass()
    {
        freeResources();
    }

    void ShadowPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_handle;
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
        std::vector<VkAttachmentDescription> attachments(m_numCascades, VkAttachmentDescription{});
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

        std::vector<VkAttachmentReference> depthAttachmentRefs(m_numCascades);
        for (uint32_t i = 0; i < depthAttachmentRefs.size(); i++)
            depthAttachmentRefs[i] = { i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        std::vector<VkSubpassDescription> subpasses(m_numCascades);
        for (size_t i = 0; i < m_numCascades; i++)
        {
            subpasses[i].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount    = 0;
            subpasses[i].pColorAttachments       = nullptr;
            subpasses[i].pDepthStencilAttachment = &depthAttachmentRefs[i];
        }

        std::vector<VkSubpassDependency> dependencies(m_numCascades);
        for (size_t i = 0; i < m_numCascades; i++)
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

        vkCreateRenderPass(m_device->getHandle(), &renderPassInfo, nullptr, &m_handle);
    }

    void ShadowPass::createResources()
    {
        m_renderTargets.resize(m_numCascades);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        for (auto& rt : m_renderTargets)
            rt = std::make_shared<Texture>(m_renderer, extent, numLayers, m_depthFormat,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            for (auto& rt : m_renderTargets)
                rt->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, VulkanRenderer::NumVirtualFrames);
        });

        m_renderTargetViews.resize(m_numCascades, std::vector<std::shared_ptr<TextureView>>(VulkanRenderer::NumVirtualFrames));
        m_framebuffers.resize(VulkanRenderer::NumVirtualFrames);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            std::vector<VkImageView> attachmentViews;
            for (uint32_t j = 0; j < m_numCascades; j++)
            {
                m_renderTargetViews[j][i] = m_renderTargets[j]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
                attachmentViews.push_back(m_renderTargetViews[j][i]->getHandle());
            }

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = m_handle;
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