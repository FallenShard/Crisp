#include "ForwardLightingPass.hpp"

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

namespace crisp
{
// ForwardLightingPass::ForwardLightingPass(const VulkanDevice& device, VkExtent2D renderArea,
//     VkSampleCountFlagBits sampleCount)
//     : VulkanRenderPass(device, true, 1)
//{
//     m_renderArea = renderArea;
//     m_defaultSampleCount = sampleCount;
//
//     /*RenderPassBuilder builder{};
//     if (m_defaultSampleCount == VK_SAMPLE_COUNT_1_BIT)
//     {
//         std::tie(m_handle, m_attachmentDescriptions) =
//             builder.addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
//                 .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
//                 .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
//                 .addAttachment(VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
//                 .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
//                 .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
//                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//                 .setNumSubpasses(1)
//                 .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
//                 .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//                 .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
//                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
//                 .create(device.getHandle());
//         m_renderTargetInfos.resize(m_attachmentDescriptions.size());
//         setColorRenderTargetInfo(0, VK_IMAGE_USAGE_SAMPLED_BIT);
//         setDepthRenderTargetInfo(1, 0);
//     }
//     else
//     {
//         std::tie(m_handle, m_attachmentDescriptions) =
//             builder.addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, m_defaultSampleCount)
//                 .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
//                 .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
//                 .addAttachment(VK_FORMAT_D32_SFLOAT, m_defaultSampleCount)
//                 .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
//                 .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
//                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//                 .addAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
//                 .setAttachmentOps(2, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE)
//                 .setAttachmentLayouts(2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
//                 .setNumSubpasses(1)
//                 .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
//                 .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
//                 .addResolveAttachmentRef(0, 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
//                 .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
//                     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
//                 .create(device.getHandle());
//         m_renderTargetInfos.resize(m_attachmentDescriptions.size());
//         setColorRenderTargetInfo(0, 0);
//         m_renderTargetInfos.at(0).initDstStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//         setDepthRenderTargetInfo(1, 0);
//         setColorRenderTargetInfo(2, VK_IMAGE_USAGE_SAMPLED_BIT);
//     }*/
//
//     createResources(device);
// }
//
// void ForwardLightingPass::createResources(const VulkanDevice& device)
//{
//     if (m_defaultSampleCount == VK_SAMPLE_COUNT_1_BIT)
//     {
//         createRenderTargets(device);
//     }
//     else
//     {
//         m_renderTargets.resize(3);
//         VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
//         auto numLayers = RendererConfig::VirtualFrameCount;
//
//         VkImageCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
//         createInfo.flags = 0;
//         createInfo.imageType = VK_IMAGE_TYPE_2D;
//         createInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
//         createInfo.extent = extent;
//         createInfo.mipLevels = 1;
//         createInfo.arrayLayers = 1;
//         createInfo.samples = m_defaultSampleCount;
//         createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         createInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
//         createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//         VkImageCreateInfo depthInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
//         depthInfo.flags = 0;
//         depthInfo.imageType = VK_IMAGE_TYPE_2D;
//         depthInfo.format = VK_FORMAT_D32_SFLOAT;
//         depthInfo.extent = extent;
//         depthInfo.mipLevels = 1;
//         depthInfo.arrayLayers = 1;
//         depthInfo.samples = m_defaultSampleCount;
//         depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
//         depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//         VkImageCreateInfo resolveInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
//         resolveInfo.flags = 0;
//         resolveInfo.imageType = VK_IMAGE_TYPE_2D;
//         resolveInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
//         resolveInfo.extent = extent;
//         resolveInfo.mipLevels = 1;
//         resolveInfo.arrayLayers = numLayers;
//         resolveInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//         resolveInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
//         resolveInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
//         resolveInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
//         resolveInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//
//         m_renderTargets[0] = std::make_unique<VulkanImage>(device, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);
//         m_renderTargets[1] = std::make_unique<VulkanImage>(device, depthInfo, VK_IMAGE_ASPECT_DEPTH_BIT);
//         m_renderTargets[2] = std::make_unique<VulkanImage>(device, resolveInfo, VK_IMAGE_ASPECT_COLOR_BIT);
//
//         /*renderer.enqueueResourceUpdate(
//             [this](VkCommandBuffer cmdBuffer)
//             {
//                 m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
//                 m_renderTargets[1]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
//                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
//                 m_renderTargets[2]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
//             });*/
//
//         m_renderTargetViews.resize(3);
//         m_renderTargetViews[0].resize(1);
//         m_renderTargetViews[1].resize(1);
//         m_renderTargetViews[2].resize(numLayers);
//
//         m_framebuffers.resize(numLayers);
//         for (uint32_t i = 0; i < numLayers; i++)
//         {
//             if (i == 0)
//             {
//                 m_renderTargetViews[0][0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D);
//                 m_renderTargetViews[1][0] = m_renderTargets[1]->createView(VK_IMAGE_VIEW_TYPE_2D);
//             }
//
//             m_renderTargetViews[2][i] = m_renderTargets[2]->createView(VK_IMAGE_VIEW_TYPE_2D, i, 1);
//
//             auto attachmentViews = { m_renderTargetViews[0][0]->getHandle(), m_renderTargetViews[1][0]->getHandle(),
//                 m_renderTargetViews[2][i]->getHandle() };
//             m_framebuffers[i] = std::make_unique<VulkanFramebuffer>(device, m_handle, m_renderArea, attachmentViews);
//         }
//     }
// }

std::unique_ptr<VulkanRenderPass> createForwardLightingPass(const VulkanDevice& device, VkExtent2D renderArea)
{
    return RenderPassBuilder()
        .setSwapChainDependency(true)
        .setRenderTargetsBuffered(true)

        .setRenderTargetCount(2)
        .setRenderTargetFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT)
        .setRenderTargetFormat(1, VK_FORMAT_D32_SFLOAT)
        .configureDepthRenderTarget(1, 0)

        .setAttachmentCount(2)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setAttachmentMapping(1, 1)
        .setAttachmentOps(1, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
        .setAttachmentLayouts(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .setDepthAttachmentRef(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea);
}
} // namespace crisp