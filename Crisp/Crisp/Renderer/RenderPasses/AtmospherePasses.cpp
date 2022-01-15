#include <Crisp/Renderer/RenderPasses/AtmospherePasses.hpp>

#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{
std::unique_ptr<VulkanRenderPass> createTransmittanceLutPass(const VulkanDevice& device)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(false)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R16G16B16A16_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, { 256, 64 });
}

std::unique_ptr<VulkanRenderPass> createSkyViewLutPass(const VulkanDevice& device)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(false)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R16G16B16A16_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, { 192, 108 });
}

std::unique_ptr<VulkanRenderPass> createSkyVolumePass(const VulkanDevice& device)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(false)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R16G16B16A16_SFLOAT)
        .setRenderTargetDepthSlices(0, 64, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        .setRenderTargetBuffered(0, false)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0, 0, 32)
        .setAttachmentBufferOverDepthSlices(0, true)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, { 32, 32 });
}

std::unique_ptr<VulkanRenderPass> createRayMarchingPass(const VulkanDevice& device, VkExtent2D renderArea)
{
    return RenderPassBuilder()
        .setRenderTargetsBuffered(true)
        .setSwapChainDependency(true)
        .setRenderTargetCount(1)
        .setRenderTargetFormat(0, VK_FORMAT_R16G16B16A16_SFLOAT)
        .configureColorRenderTarget(0, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea);
}

} // namespace crisp
