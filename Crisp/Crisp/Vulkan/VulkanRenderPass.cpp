#include "VulkanRenderPass.hpp"

#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanFramebuffer.hpp>
#include <Crisp/Vulkan/VulkanImage.hpp>
#include <Crisp/Vulkan/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanSwapChain.hpp>

#include <CrispCore/Enumerate.hpp>

namespace crisp
{
namespace
{
Result<VkExtent2D> getRenderAreaFromDescription(Renderer& renderer, RenderPassDescription& description)
{
    if (description.isSwapChainDependent)
    {
        return renderer.getSwapChain().getExtent();
    }

    if (!description.renderArea)
    {
        return resultError("renderArea wasn't specified for this render pass!");
    }

    return *description.renderArea;
}
} // namespace

VulkanRenderPass::VulkanRenderPass(Renderer& renderer, bool isWindowSizeDependent, uint32_t numSubpasses)
    : VulkanResource(renderer.getDevice().getResourceDeallocator())
    , m_isWindowSizeDependent(isWindowSizeDependent)
    , m_renderArea(renderer.getSwapChain().getExtent())
    , m_numSubpasses(numSubpasses)
    , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
{
}

VulkanRenderPass::VulkanRenderPass(Renderer& renderer, VkRenderPass renderPass, RenderPassDescription&& description)
    : VulkanResource(renderPass, renderer.getDevice().getResourceDeallocator())
    , m_isWindowSizeDependent(description.isSwapChainDependent)
    , m_renderArea(getRenderAreaFromDescription(renderer, description).unwrap())
    , m_numSubpasses(description.subpassCount)
    , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
    , m_attachmentDescriptions(std::move(description.attachmentDescriptions))
    , m_renderTargetInfos(std::move(description.renderTargetInfos))
    , m_bufferedRenderTargets(description.bufferedRenderTargets)
{
    m_attachmentClearValues.resize(m_attachmentDescriptions.size());
    if (m_attachmentClearValues.size() != m_renderTargetInfos.size())
    {
        spdlog::critical("Mismatch between render targets and attachments! Fix this...");
        std::exit(-1);
    }
    for (const auto& [i, clearValue] : enumerate(m_attachmentClearValues))
    {
        clearValue = m_renderTargetInfos.at(i).clearValue;
    }

    createResources(renderer);
}

VulkanRenderPass::~VulkanRenderPass()
{
    freeResources();
}

void VulkanRenderPass::recreate(Renderer& renderer)
{
    if (m_isWindowSizeDependent)
    {
        freeResources();

        m_renderArea = renderer.getSwapChainExtent();
        createResources(renderer);
    }
}

VkExtent2D VulkanRenderPass::getRenderArea() const
{
    return m_renderArea;
}

VkViewport VulkanRenderPass::createViewport() const
{
    return { 0.0f, 0.0f, static_cast<float>(m_renderArea.width), static_cast<float>(m_renderArea.height), 0.0f, 1.0f };
}

VkRect2D VulkanRenderPass::createScissor() const
{
    return {
        {0, 0},
        m_renderArea
    };
}

void VulkanRenderPass::begin(const VkCommandBuffer cmdBuffer, const uint32_t frameIndex,
    const VkSubpassContents content) const
{
    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = m_framebuffers.at(frameIndex)->getHandle();
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_renderArea;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(m_attachmentClearValues.size());
    renderPassInfo.pClearValues = m_attachmentClearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, content);
}

void VulkanRenderPass::end(const VkCommandBuffer cmdBuffer, const uint32_t frameIndex)
{
    vkCmdEndRenderPass(cmdBuffer);

    if (m_renderTargetViews.empty())
    {
        return;
    }

    for (const auto& [i, renderTargetView] : enumerate(m_renderTargetViews.at(frameIndex)))
    {
        auto& image = renderTargetView->getImage();
        image.setImageLayout(m_attachmentDescriptions.at(i).finalLayout, renderTargetView->getSubresourceRange());
    }
}

void VulkanRenderPass::nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents) const
{
    vkCmdNextSubpass(cmdBuffer, contents);
}

VulkanImage& VulkanRenderPass::getRenderTarget(const uint32_t index) const
{
    return *m_renderTargets.at(index);
}

const VulkanImageView& VulkanRenderPass::getRenderTargetView(const uint32_t index, const uint32_t frameIndex) const
{
    return *m_renderTargetViews.at(frameIndex).at(index);
}

std::vector<VulkanImageView*> VulkanRenderPass::getRenderTargetViews(uint32_t renderTargetIndex) const
{
    std::vector<VulkanImageView*> imageViews(m_renderTargetViews.size());
    for (uint32_t i = 0; i < m_renderTargetViews.size(); ++i)
        imageViews.at(i) = m_renderTargetViews.at(i).at(renderTargetIndex).get();
    return imageViews;
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(const VulkanDevice& device, uint32_t index,
    uint32_t numFrames) const
{
    return std::make_unique<VulkanImageView>(device, *m_renderTargets.at(index),
        numFrames > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, 0, numFrames, 0, 1);
}

std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(const VulkanDevice& device, uint32_t index,
    uint32_t baseLayer, uint32_t numLayers) const
{
    return std::make_unique<VulkanImageView>(device, *m_renderTargets.at(index),
        numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, baseLayer, numLayers);
}

std::unique_ptr<VulkanImage> VulkanRenderPass::extractRenderTarget(uint32_t index)
{
    std::unique_ptr<VulkanImage> image = std::move(m_renderTargets.at(index));
    m_renderTargets.erase(m_renderTargets.begin() + index);
    return image;
}

VkSampleCountFlagBits VulkanRenderPass::getDefaultSampleCount() const
{
    return m_defaultSampleCount;
}

void VulkanRenderPass::createRenderTargets(Renderer& renderer)
{
    const VkExtent3D extent = { m_renderArea.width, m_renderArea.height, 1u };
    const auto layerCount = m_bufferedRenderTargets ? RendererConfig::VirtualFrameCount : 1;

    m_renderTargets.resize(m_attachmentDescriptions.size());
    for (const auto& [i, attachment] : enumerate(m_attachmentDescriptions))
    {
        m_renderTargets[i] = std::make_unique<VulkanImage>(renderer.getDevice(), extent, layerCount, 1,
            attachment.format, m_renderTargetInfos.at(i).usage, determineImageAspect(attachment.format), 0);
    }

    renderer.enqueueResourceUpdate(
        [this](VkCommandBuffer cmdBuffer)
        {
            for (const auto& [i, renderTarget] : enumerate(m_renderTargets))
            {
                const auto& desc = m_attachmentDescriptions.at(i);
                const auto& info = m_renderTargetInfos.at(i);
                renderTarget->transitionLayout(cmdBuffer, desc.initialLayout, info.initSrcStageFlags,
                    info.initDstStageFlags);
            }
        });

    createRenderTargetViewsAndFramebuffers(renderer.getDevice(), layerCount);
}

void VulkanRenderPass::createRenderTargetViewsAndFramebuffers(const VulkanDevice& device, const uint32_t layerCount)
{
    const size_t renderTargetCount = m_attachmentDescriptions.size();
    m_renderTargetViews.resize(layerCount);
    m_framebuffers.resize(layerCount);
    for (const auto& [i, renderTargetViews] : enumerate(m_renderTargetViews))
    {
        renderTargetViews.resize(renderTargetCount);

        std::vector<VkImageView> renderTargetViewHandles(renderTargetCount);
        for (const auto& [j, renderTarget] : enumerate(m_renderTargets))
        {
            renderTargetViews.at(j) = renderTarget->createView(VK_IMAGE_VIEW_TYPE_2D, static_cast<uint32_t>(i), 1);
            renderTargetViewHandles.at(j) = renderTargetViews.at(j)->getHandle();
        }

        m_framebuffers[i] =
            std::make_unique<VulkanFramebuffer>(device, m_handle, m_renderArea, renderTargetViewHandles);
    }
}

VkExtent3D VulkanRenderPass::getRenderAreaExtent() const
{
    return { m_renderArea.width, m_renderArea.height, 1u };
}

void VulkanRenderPass::createResources(Renderer& renderer)
{
    createRenderTargets(renderer);
}

void VulkanRenderPass::freeResources()
{
    m_renderTargets.clear();
    m_renderTargetViews.clear();
    m_framebuffers.clear();
}

void VulkanRenderPass::setDepthRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
    VkClearDepthStencilValue clearValue)
{
    m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | additionalFlags;

    if (m_attachmentClearValues.size() <= index)
    {
        m_attachmentClearValues.resize(index + 1);
    }
    m_attachmentClearValues.at(index).depthStencil = clearValue;
}

void VulkanRenderPass::setColorRenderTargetInfo(uint32_t index, VkImageUsageFlags additionalFlags,
    VkClearColorValue clearValue)
{
    m_renderTargetInfos.at(index).initDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    m_renderTargetInfos.at(index).usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | additionalFlags;

    if (m_attachmentClearValues.size() <= index)
    {
        m_attachmentClearValues.resize(index + 1);
    }
    m_attachmentClearValues.at(index).color = clearValue;
}
} // namespace crisp
