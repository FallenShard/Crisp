#include "VarianceShadowMapPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>

namespace crisp
{
    VarianceShadowMapPass::VarianceShadowMapPass(Renderer* renderer, unsigned int shadowMapSize)
        : VulkanRenderPass(renderer, false, 1)
    {
        m_clearValues.resize(2);
        m_clearValues[0].color        = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_clearValues[1].depthStencil = { 1.0f, 0 };

        m_renderArea = { shadowMapSize, shadowMapSize };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R32G32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .create(m_renderer->getDevice().getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void VarianceShadowMapPass::createResources()
    {
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = RendererConfig::VirtualFrameCount;

        m_renderTargets.resize(2);
        m_renderTargets[0] = std::make_unique<VulkanImage>(m_renderer->getDevice(), extent, numLayers, 1, VK_FORMAT_R32G32_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0);
        m_renderTargets[1] = std::make_unique<VulkanImage>(m_renderer->getDevice(), extent, numLayers, 1, VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            m_renderTargets[1]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
        });

        m_renderTargetViews.resize(2);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(numLayers);

        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            std::vector<VkImageView> attachmentViews;
            for (uint32_t j = 0; j < 2; j++)
            {
                m_renderTargetViews[j][i] = m_renderTargets[j]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
                attachmentViews.push_back(m_renderTargetViews[j][i]->getHandle());
            }

            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_renderer->getDevice(), m_handle, m_renderArea, attachmentViews);
        }
    }
}