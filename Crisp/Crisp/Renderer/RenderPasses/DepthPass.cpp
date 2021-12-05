#include "DepthPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanFramebuffer.hpp>

namespace crisp
{
    DepthPass::DepthPass(Renderer* renderer)
        : VulkanRenderPass(renderer, true, 1)
    {
        m_clearValues.resize(RenderTarget::Count);
        m_clearValues[0].depthStencil = { 0.0f, 0 };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(Depth, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(Depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .setDepthAttachmentRef(0, Depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());
        m_finalLayouts = { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void DepthPass::createResources()
    {
        m_renderArea = m_renderer->getSwapChainExtent();
        m_renderTargets.resize(RenderTarget::Count);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = RendererConfig::VirtualFrameCount;

        m_renderTargets[Depth] = std::make_unique<VulkanImage>(m_device, extent, numLayers, 1, VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[Depth]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });

        m_renderTargetViews.resize(Count);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(numLayers);

        m_framebuffers.resize(numLayers);
        for (uint32_t i = 0; i < numLayers; i++)
        {
            m_renderTargetViews[Depth][i] = m_renderTargets[Depth]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            auto attachmentViews =
            {
                m_renderTargetViews[Depth][i]->getHandle()
            };
            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, attachmentViews);
        }
    }
}