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
std::unique_ptr<VulkanRenderPass> createTransmittanceLutPass(Renderer& renderer)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = false;
    description.renderArea = { 256, 64 };
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

std::unique_ptr<VulkanRenderPass> createSkyViewLutPass(Renderer& renderer)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = false;
    description.renderArea = { 192, 108 };
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

CameraVolumesPass::CameraVolumesPass(Renderer& renderer)
    : VulkanRenderPass(renderer, false, 1)
{
    m_renderArea = { 32, 32 };

    RenderPassBuilder builder{};
    std::tie(m_handle, m_attachmentDescriptions) =
        builder.addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());
    m_renderTargetInfos.resize(m_attachmentDescriptions.size());
    setColorRenderTargetInfo(0, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    createResources(renderer);
}

void CameraVolumesPass::createResources(Renderer& renderer)
{
    m_renderTargets.resize(1);
    VkExtent3D extent{ m_renderArea.width, m_renderArea.height, 32u };

    m_renderTargets[0] =
        std::make_unique<VulkanImage>(renderer.getDevice(), extent, 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT);

    renderer.enqueueResourceUpdate(
        [this](VkCommandBuffer cmdBuffer)
        {
            m_renderTargets[0]->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        });

    m_arrayViews.resize(2);
    m_arrayViews[0] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 32);
    m_arrayViews[1] = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 32);

    const uint32_t layerCount = RendererConfig::VirtualFrameCount;
    m_renderTargetViews.resize(layerCount);
    m_framebuffers.resize(layerCount);
    for (const auto& [i, renderTargetViews] : enumerate(m_renderTargetViews))
    {
        renderTargetViews.resize(1);
        std::vector<VkImageView> viewHandles(1);
        for (uint32_t j = 0; j < 1; ++j)
        {
            renderTargetViews.at(j) = m_renderTargets[0]->createView(VK_IMAGE_VIEW_TYPE_2D, 0, 32);
            viewHandles.at(j) = renderTargetViews.at(j)->getHandle();
        }
        m_framebuffers.at(i) =
            std::make_unique<VulkanFramebuffer>(renderer.getDevice(), m_handle, m_renderArea, viewHandles, 32);
    }
}

std::unique_ptr<VulkanRenderPass> createRayMarchingPass(Renderer& renderer)
{
    auto [handle, attachmentDescriptions] =
        RenderPassBuilder()
            .addAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT)
            .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

            .setNumSubpasses(1)
            .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            .addDependency(VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            .create(renderer.getDevice().getHandle());

    RenderPassDescription description{};
    description.isSwapChainDependent = true;
    description.subpassCount = 1;
    description.attachmentDescriptions = std::move(attachmentDescriptions);
    description.renderTargetInfos.resize(description.attachmentDescriptions.size());
    description.renderTargetInfos[0].configureColorRenderTarget(
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    return std::make_unique<VulkanRenderPass>(renderer, handle, std::move(description));
}

} // namespace crisp
