#include "AtmosphericLutPass.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderPassBuilder.hpp"
#include "Vulkan/VulkanImage.hpp"
#include "Vulkan/VulkanImageView.hpp"
#include "vulkan/VulkanFramebuffer.hpp"

namespace crisp
{
    TransmittanceLutPass::TransmittanceLutPass(Renderer* renderer)
        : VulkanRenderPass(renderer, false, 1)
    {
        m_renderArea = { 256, 64 };
        m_clearValues.resize(1);
        for (int i = 0; i < 1; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void TransmittanceLutPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, Renderer::NumVirtualFrames, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });


        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(Renderer::NumVirtualFrames);

        m_framebuffers.resize(Renderer::NumVirtualFrames);
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            auto renderTargetViews =
            {
                m_renderTargetViews[0][i]->getHandle()
            };
            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, renderTargetViews);
        }
    }

    SkyViewLutPass::SkyViewLutPass(Renderer* renderer)
        : VulkanRenderPass(renderer, false, 1)
    {
        m_renderArea = { 192, 108 };
        m_clearValues.resize(1);
        for (int i = 0; i < 1; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void SkyViewLutPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, Renderer::NumVirtualFrames, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });


        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(Renderer::NumVirtualFrames);

        m_framebuffers.resize(Renderer::NumVirtualFrames);
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            auto renderTargetViews =
            {
                m_renderTargetViews[0][i]->getHandle()
            };
            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, renderTargetViews);
        }
    }

    CameraVolumesPass::CameraVolumesPass(Renderer* renderer)
        : VulkanRenderPass(renderer, false, 1)
    {
        m_renderArea = { 32, 32 };
        m_clearValues.resize(1);
        for (int i = 0; i < 1; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void CameraVolumesPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent{ m_renderArea.width, m_renderArea.height, 32u };

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });

        m_arrayViews.resize(2);
        m_arrayViews[0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 32);
        m_arrayViews[1] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 32);

        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(Renderer::NumVirtualFrames);

        m_framebuffers.resize(Renderer::NumVirtualFrames);
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 32);

            auto renderTargetViews =
            {
                m_renderTargetViews[0][i]->getHandle()
            };
            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, renderTargetViews, 32);
        }
    }

    RayMarchingPass::RayMarchingPass(Renderer* renderer)
        : VulkanRenderPass(renderer, true, 1)
    {
        m_renderArea = m_renderer->getSwapChainExtent();
        m_clearValues.resize(1);
        for (int i = 0; i < 1; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_device->getHandle());

        m_finalLayouts = { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        createResources();
    }

    void RayMarchingPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, Renderer::NumVirtualFrames, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });


        m_renderTargetViews.resize(1);
        for (auto& rtv : m_renderTargetViews)
            rtv.resize(Renderer::NumVirtualFrames);

        m_framebuffers.resize(Renderer::NumVirtualFrames);
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; i++)
        {
            m_renderTargetViews[0][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);

            auto renderTargetViews =
            {
                m_renderTargetViews[0][i]->getHandle()
            };
            m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(m_device, m_handle, m_renderArea, renderTargetViews);
        }
    }
}
