#include "CubeMapRenderPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanFramebuffer.hpp>
#include <CrispCore/Image/Image.hpp>

namespace crisp
{
    CubeMapRenderPass::CubeMapRenderPass(Renderer* renderer, VkExtent2D renderArea, bool allocateMipmaps)
        : VulkanRenderPass(renderer, false, 1)
        , m_allocateMipmaps(allocateMipmaps)
    {
        m_renderArea = renderArea;
        m_clearValues.resize(6);
        for (int i = 0; i < 6; i++)
            m_clearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };

        RenderPassBuilder builder;
        for (int i = 0; i < 6; i++)
        {
            builder.addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
                .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        m_handle = builder
            .setNumSubpasses(6)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(1, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(2, 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(3, 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(4, 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(5, 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(m_renderer->getDevice()->getHandle());

        m_finalLayouts.resize(6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        createResources();
    }

    std::unique_ptr<VulkanImage> CubeMapRenderPass::extractRenderTarget(uint32_t index)
    {
        std::unique_ptr<VulkanImage> image = std::move(m_renderTargets[index]);
        m_renderTargets.clear();
        return image;
    }

    void CubeMapRenderPass::createResources()
    {
        m_renderTargets.resize(1);
        VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
        auto numLayers = 6;
        auto mipmapCount = !m_allocateMipmaps ? 1 : Image::getMipLevels(extent.width, extent.height);
        auto additionalFlags = mipmapCount == 1 ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT; // for mipmap transfers

        m_renderTargets[0] = std::make_unique<VulkanImage>(m_device, extent, numLayers, mipmapCount, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | additionalFlags, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

        m_renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer)
        {
            VkImageSubresourceRange range = {};
            range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseArrayLayer = 0;
            range.layerCount     = 6;
            range.baseMipLevel   = 0;
            range.levelCount     = 1;
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

        m_renderTargetViews.resize(6);
        std::vector<VkImageView> renderTargetViewHandles;
        for (int i = 0; i < 6; ++i)
        {
            m_renderTargetViews[i].resize(1);
            m_renderTargetViews[i][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
            renderTargetViewHandles.push_back(m_renderTargetViews[i][0]->getHandle());
        }

        m_framebuffers.resize(1);
        m_framebuffers[0] = std::make_unique<VulkanFramebuffer>(m_renderer->getDevice(), m_handle, m_renderArea, renderTargetViewHandles);
    }
}