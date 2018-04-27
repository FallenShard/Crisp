#include "ShadowPass.hpp"

#include "Renderer/VulkanRenderer.hpp"
#include "Renderer/RenderPassBuilder.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace crisp
{
    ShadowPass::ShadowPass(VulkanRenderer* renderer, unsigned int shadowMapSize, unsigned int numCascades)
        : VulkanRenderPass(renderer)
        , m_numCascades(numCascades)
    {
        m_clearValues.resize(numCascades);
        for (auto& clearValue : m_clearValues)
            clearValue.depthStencil = { 1.0f, 0 };

        m_renderArea = { shadowMapSize, shadowMapSize };

        RenderPassBuilder builder;
        for (uint32_t i = 0; i < m_numCascades; ++i)
            builder.addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
                .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        for (uint32_t i = 0; i < m_numCascades; ++i)
            builder.addSubpass().setDepthAttachmentRef(i, i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        m_handle = builder
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = builder.getFinalLayouts();

        createResources();
    }

    void ShadowPass::createResources()
    {
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = VulkanRenderer::NumVirtualFrames;

        m_renderTargets.resize(m_numCascades);
        for (auto& rt : m_renderTargets)
            rt = std::make_unique<VulkanImage>(m_device, extent, numLayers, 1, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            for (auto& rt : m_renderTargets)
                rt->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(m_numCascades);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(numLayers);

        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            std::vector<VkImageView> attachmentViews;
            for (uint32_t j = 0; j < m_numCascades; j++)
            {
                m_renderTargetViews[j][i] = m_renderTargets[j]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
                attachmentViews.push_back(m_renderTargetViews[j][i]->getHandle());
            }

            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, attachmentViews);
        }
    }
}