#include "EnvironmentRenderPass.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderPassBuilder.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace crisp
{
    EnvironmentRenderPass::EnvironmentRenderPass(Renderer* renderer, VkExtent2D renderArea)
        : VulkanRenderPass(renderer, false, 1)
    {
        m_renderArea = renderArea;
        m_clearValues.resize(6);
        for (int i = 0; i < 6; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(2, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(3, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(4, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(5, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(6)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(1, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(2, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(3, 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(4, 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(5, 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());
        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void EnvironmentRenderPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = 6;

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, numLayers, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(6);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(1);

        m_framebuffers.resize(1);
        m_renderTargetViews[0][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 0, 1);
        m_renderTargetViews[1][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 1, 1);
        m_renderTargetViews[2][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 2, 1);
        m_renderTargetViews[3][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 3, 1);
        m_renderTargetViews[4][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 4, 1);
        m_renderTargetViews[5][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_CUBE, 5, 1);
        auto renderTargetViews =
        {
            m_renderTargetViews[0][0]->getHandle(),
            m_renderTargetViews[1][0]->getHandle(),
            m_renderTargetViews[2][0]->getHandle(),
            m_renderTargetViews[3][0]->getHandle(),
            m_renderTargetViews[4][0]->getHandle(),
            m_renderTargetViews[5][0]->getHandle()
        };
        m_framebuffers[0] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, renderTargetViews);
    }
}