#include "TexturePass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>

namespace crisp
{
    TexturePass::TexturePass(Renderer* renderer, VkExtent2D renderArea, VkFormat textureFormat)
        : VulkanRenderPass(renderer, false, 1)
        , m_textureFormat(textureFormat)
    {
        m_renderArea = renderArea;
        m_clearValues.resize(1);
        for (int i = 0; i < 1; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        m_handle = RenderPassBuilder()
            .addAttachment(m_textureFormat, VK_SAMPLE_COUNT_1_BIT)
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

    std::unique_ptr<VulkanImage> TexturePass::extractRenderTarget(uint32_t index)
    {
        std::unique_ptr<VulkanImage> image = std::move(m_renderTargets[index]);
        m_renderTargets.clear();
        return image;
    }

    void TexturePass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = 1;

        m_renderTargets[0] = std::make_unique<VulkanImage>(*m_device, extent, numLayers, 1, m_textureFormat,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, 0);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
            {
                m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            });

        m_renderTargetViews.resize(numLayers);
        std::vector<VkImageView> renderTargetViewHandles;
        for (int i = 0; i < numLayers; ++i)
        {
            m_renderTargetViews[i].resize(1);
            m_renderTargetViews[i][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            renderTargetViewHandles.push_back(m_renderTargetViews[i][0]->getHandle());
        }

        m_framebuffers.resize(1);
        m_framebuffers[0] = std::make_unique<VulkanFramebuffer>(*m_device, m_handle, m_renderArea, renderTargetViewHandles);
    }
}