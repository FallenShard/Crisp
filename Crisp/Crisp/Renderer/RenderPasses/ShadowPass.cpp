#include "ShadowPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>

namespace crisp
{
    ShadowPass::ShadowPass(Renderer* renderer, unsigned int shadowMapSize, unsigned int numCascades)
        : VulkanRenderPass(renderer, false, numCascades)
        , m_numCascades(numCascades)
    {
        m_renderArea = { shadowMapSize, shadowMapSize };

        m_clearValues.resize(m_numCascades);
        for (auto& clearValue : m_clearValues)
            clearValue.depthStencil = { 1.0f, 0 };

        RenderPassBuilder builder;
        builder.setNumSubpasses(m_numCascades)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        for (uint32_t i = 0; i < m_numCascades; ++i)
        {
            builder.addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
                .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .setDepthAttachmentRef(i, i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        m_finalLayouts.push_back(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        m_handle = builder.create(m_renderer->getDevice()->getHandle());

        createResources();
    }

    void ShadowPass::createResources()
    {
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = RendererConfig::VirtualFrameCount;

        m_renderTargets.resize(1);
        m_renderTargets[0] = std::make_unique<VulkanImage>(m_renderer->getDevice(), extent, m_numCascades * numLayers, 1, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(numLayers);

        m_individualLayerViews.resize(m_numCascades);
        for (auto& rtv : m_individualLayerViews)
            rtv.resize(numLayers);

        VkImageViewType type = (m_numCascades == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            std::vector<VkImageView> attachmentViews;
            for (uint32_t a = 0; a < m_numCascades; ++a)
            {
                m_individualLayerViews[a][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i * m_numCascades + a, 1);
                attachmentViews.push_back(m_individualLayerViews[a][i]->getHandle());
            }


            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(type, i * m_numCascades, m_numCascades);

            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_renderer->getDevice(), m_handle, m_renderArea, attachmentViews);
        }
    }
}