#include "ShadowPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{
// ShadowPass::ShadowPass(const VulkanDevice& device, unsigned int shadowMapSize, unsigned int numCascades)
//     : VulkanRenderPass(device, false, numCascades)
//     , m_numCascades(numCascades)
//{
//     m_renderArea = { shadowMapSize, shadowMapSize };
//
//     RenderPassBuilder builder;
//     builder.setNumSubpasses(m_numCascades)
//         .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_SHADER_READ_BIT,
//             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
//
//     for (uint32_t i = 0; i < m_numCascades; ++i)
//     {
//         builder.addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
//             .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
//             .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//             .setDepthAttachmentRef(i, i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
//     }
//     std::tie(m_handle, m_attachmentDescriptions) = builder.create(device.getHandle());
//     m_renderTargetInfos.resize(m_attachmentDescriptions.size());
//     setDepthRenderTargetInfo(0, VK_IMAGE_USAGE_SAMPLED_BIT, { 1.0f, 0 });
//     m_attachmentClearValues.resize(m_attachmentDescriptions.size());
//     for (uint32_t i = 0; i < m_numCascades; ++i)
//     {
//         m_attachmentClearValues.at(i).depthStencil = { 1.0f, 0 };
//     }
//
//     createResources(device);
// }
//
// void ShadowPass::createResources(const VulkanDevice& device)
//{
//     const VkExtent3D extent{ m_renderArea.width, m_renderArea.height, 1u };
//     const uint32_t layerCount{ RendererConfig::VirtualFrameCount };
//
//     m_renderTargets.resize(1);
//     m_renderTargets[0] =
//         std::make_unique<VulkanImage>(device, extent, m_numCascades * layerCount, 1, VK_FORMAT_D32_SFLOAT,
//             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, 0);
//
//     /*renderer.enqueueResourceUpdate(
//         [this](VkCommandBuffer cmdBuffer)
//         {
//             m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//         });*/
//
//     const VkImageViewType type = (m_numCascades == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
//
//     m_individualLayerViews.resize(layerCount);
//     m_renderTargetViews.resize(layerCount);
//     m_framebuffers.resize(layerCount);
//     for (const auto& [layerIdx, renderTargetViews] : enumerate(m_renderTargetViews))
//     {
//         renderTargetViews.resize(1);
//         m_individualLayerViews.at(layerIdx).resize(m_numCascades);
//
//         for (const auto& [rtIdx, renderTarget] : enumerate(m_renderTargets))
//         {
//             renderTargetViews.at(rtIdx) =
//                 renderTarget->createView(type, static_cast<uint32_t>(layerIdx * m_numCascades), m_numCascades);
//         }
//
//         std::vector<VkImageView> renderTargetViewHandles(m_numCascades);
//         for (uint32_t i = 0; i < m_numCascades; ++i)
//         {
//             m_individualLayerViews[layerIdx][i] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D,
//                 static_cast<uint32_t>(layerIdx * m_numCascades + i), 1);
//             renderTargetViewHandles.at(i) = m_individualLayerViews[layerIdx][i]->getHandle();
//         }
//
//         m_framebuffers[layerIdx] =
//             std::make_unique<VulkanFramebuffer>(device, m_handle, m_renderArea, renderTargetViewHandles);
//     }
// }

std::unique_ptr<VulkanRenderPass> createShadowMapPass(const VulkanDevice& device, uint32_t shadowMapSize,
    uint32_t cascadeCount)
{
    RenderPassBuilder builder;
    builder.setSwapChainDependency(false)
        .setRenderTargetsBuffered(true)

        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_D32_SFLOAT)
        .setRenderTargetLayersAndMipmaps(0, cascadeCount)
        .configureDepthRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT, { 1.0f, 0 })

        .setNumSubpasses(cascadeCount)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    builder.setAttachmentCount(cascadeCount);
    for (uint32_t i = 0; i < cascadeCount; ++i)
    {
        builder.setAttachmentMapping(i, 0, i, 1)
            .setAttachmentOps(i, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(i, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            .setDepthAttachmentRef(i, i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    return builder.create(device, { shadowMapSize, shadowMapSize });
}
} // namespace crisp