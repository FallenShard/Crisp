#include "CubeMapRenderPass.hpp"

#include <Crisp/Image/Image.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/vulkan/VulkanDevice.hpp>
#include <Crisp/vulkan/VulkanFramebuffer.hpp>
#include <Crisp/vulkan/VulkanImage.hpp>

namespace crisp
{
//
// CubeMapRenderPass::CubeMapRenderPass(const VulkanDevice& device, VkExtent2D renderArea, bool allocateMipmaps)
//    : VulkanRenderPass(device, false, 1)
//    , m_allocateMipmaps(allocateMipmaps)
//{
//    m_combinedImages = true;
//    m_renderArea = renderArea;
//
//    RenderPassBuilder builder;
//    builder.setNumSubpasses(6).addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
//        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
//    for (int i = 0; i < 6; i++)
//    {
//        builder.addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
//            .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
//            .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) .addColorAttachmentRef(i, i,
//            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
//    }
//
//    std::tie(m_handle, m_attachmentDescriptions) = builder.create(device.getHandle());
//
//    m_attachmentClearValues.resize(m_attachmentDescriptions.size());
//    for (int i = 0; i < 6; i++)
//        m_attachmentClearValues[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };
//
//    createResources(device);
//}
//
// std::unique_ptr<VulkanImage> CubeMapRenderPass::extractRenderTarget(uint32_t index)
//{
//    std::unique_ptr<VulkanImage> image = std::move(m_renderTargets[index]);
//    m_renderTargets.clear();
//    return image;
//}
//
// void CubeMapRenderPass::createResources(const VulkanDevice& device)
//{
//    const VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
//    const uint32_t numLayers = 6;
//
//    m_renderTargetInfos.resize(1);
//    m_renderTargetInfos[0].configureColorRenderTarget(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
//    |
//                                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | additionalFlags);
//
//    m_renderTargets.resize(1);
//    m_renderTargets[0] = std::make_unique<VulkanImage>(device, extent, numLayers, mipmapCount,
//        VK_FORMAT_R16G16B16A16_SFLOAT, , VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
//
//    m_attachmentMappings.resize(6);
//    // m_attachmentMappings[0] =
//
//    /* renderer.enqueueResourceUpdate(
//         [this, mipmapCount](VkCommandBuffer cmdBuffer)
//         {
//             VkImageSubresourceRange range = {};
//             range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//             range.baseArrayLayer = 0;
//             range.layerCount = 6;
//             range.baseMipLevel = 0;
//             range.levelCount = 1;
//             m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, range,
//                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//         });*/
//
//    m_renderTargetViews.resize(6);
//    std::vector<VkImageView> renderTargetViewHandles;
//    for (int i = 0; i < 6; ++i)
//    {
//        m_renderTargetViews[i].resize(1);
//        m_renderTargetViews[i][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
//        renderTargetViewHandles.push_back(m_renderTargetViews[i][0]->getHandle());
//    }
//
//    m_framebuffers.resize(1);
//    m_framebuffers[0] = std::make_unique<VulkanFramebuffer>(device, m_handle, m_renderArea, renderTargetViewHandles);
//}

std::unique_ptr<VulkanRenderPass> createCubeMapPass(
    const VulkanDevice& device, VkExtent2D renderArea, bool allocateMipmaps)
{
    const auto mipmapCount = !allocateMipmaps ? 1 : Image::getMipLevels(renderArea.width, renderArea.height);
    const auto additionalFlags = mipmapCount == 1 ? 0 : VK_IMAGE_USAGE_TRANSFER_DST_BIT; // for mipmap transfers

    RenderPassBuilder builder;
    builder.setRenderTargetsBuffered(false)
        .setSwapChainDependency(false)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R16G16B16A16_SFLOAT)
        .setRenderTargetLayersAndMipmaps(0, 6)
        .configureColorRenderTarget(
            0,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                additionalFlags)
        .setRenderTargetDepthSlices(0, 1, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

    builder.setAttachmentCount(6);
    builder.setNumSubpasses(6).addDependency(
        VK_SUBPASS_EXTERNAL,
        0,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    for (int i = 0; i < 6; i++)
    {
        builder.setAttachmentMapping(i, 0, i, 1)
            .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addColorAttachmentRef(i, i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    return builder.create(device, renderArea);
}
} // namespace crisp