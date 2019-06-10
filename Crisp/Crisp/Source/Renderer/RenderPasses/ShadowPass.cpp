#include "ShadowPass.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderPassBuilder.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace crisp
{
    ShadowPass::ShadowPass(Renderer* renderer, unsigned int shadowMapSize, unsigned int numCascades)
        : VulkanRenderPass(renderer, false, 1)
        , m_numCascades(numCascades)
    {
        m_clearValues.resize(1);
        for (auto& clearValue : m_clearValues)
            clearValue.depthStencil = { 1.0f, 0 };

        if (m_numCascades == 1)
            m_renderArea = { shadowMapSize, shadowMapSize };
        else
            m_renderArea = { shadowMapSize * 2, shadowMapSize * 2 };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .setDepthAttachmentRef(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void ShadowPass::createResources()
    {
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = Renderer::NumVirtualFrames;

        m_renderTargets.resize(1);
        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, numLayers, 1, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(numLayers);

        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            std::vector<VkImageView> attachmentViews;
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            attachmentViews.push_back(m_renderTargetViews[0][i]->getHandle());

            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, attachmentViews);
        }
    }
}