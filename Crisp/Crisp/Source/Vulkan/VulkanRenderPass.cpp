#include "VulkanRenderPass.hpp"

#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanFramebuffer.hpp"
#include "Vulkan/VulkanImage.hpp"
#include "vulkan/VulkanImageView.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    VulkanRenderPass::VulkanRenderPass(Renderer* renderer, bool isWindowSizeDependent, uint32_t numSubpasses)
        : VulkanResource(renderer->getDevice())
        , m_renderer(renderer)
        , m_isWindowSizeDependent(isWindowSizeDependent)
        , m_numSubpasses(numSubpasses)
        , m_defaultSampleCount(VK_SAMPLE_COUNT_1_BIT)
    {
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        freeResources();

        if (m_deferDestruction)
            m_device->deferDestruction(m_handle, vkDestroyRenderPass);
        else
            vkDestroyRenderPass(m_device->getHandle(), m_handle, nullptr);
    }

    void VulkanRenderPass::recreate()
    {
        if (m_isWindowSizeDependent)
        {
            freeResources();
            createResources();
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
        return { { 0, 0 }, m_renderArea };
    }

    void VulkanRenderPass::begin(VkCommandBuffer cmdBuffer) const
    {
        VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[m_renderer->getCurrentVirtualFrameIndex()]->getHandle();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderArea;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderPass::begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex) const
    {
        VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        renderPassInfo.renderPass        = m_handle;
        renderPassInfo.framebuffer       = m_framebuffers[frameIndex]->getHandle();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_renderArea;
        renderPassInfo.clearValueCount   = static_cast<uint32_t>(m_clearValues.size());
        renderPassInfo.pClearValues      = m_clearValues.data();

        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderPass::end(VkCommandBuffer cmdBuffer, uint32_t frameIndex) const
    {
        vkCmdEndRenderPass(cmdBuffer);

        for (uint32_t i = 0; i < m_renderTargetViews.size(); ++i)
        {
            if (frameIndex < m_renderTargetViews[i].size())
            {
                auto& attachmentView = m_renderTargetViews[i][frameIndex];
                auto& image = const_cast<VulkanImage&>(attachmentView->getImage());
                image.setImageLayout(m_finalLayouts[i], attachmentView->getSubresourceRange());
            }
        }
    }

    void VulkanRenderPass::nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents) const
    {
        vkCmdNextSubpass(cmdBuffer, contents);
    }

    VulkanImage* VulkanRenderPass::getRenderTarget(unsigned int index) const
    {
        return m_renderTargets.at(index).get();
    }

    const VulkanImageView& VulkanRenderPass::getRenderTargetView(unsigned int index, unsigned int frameIndex) const
    {
        return *m_renderTargetViews.at(index).at(frameIndex);
    }

    std::vector<VulkanImageView*> VulkanRenderPass::getRenderTargetViews(unsigned int renderTargetIndex) const
    {
        std::vector<VulkanImageView*> imageViews;
        for (uint32_t i = 0; i < Renderer::NumVirtualFrames; ++i)
            imageViews.push_back(m_renderTargetViews.at(renderTargetIndex).at(i).get());
        return imageViews;
    }

    std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(unsigned int index, unsigned int numFrames) const
    {
        return std::make_unique<VulkanImageView>(m_renderer->getDevice(), *m_renderTargets.at(index), numFrames > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, 0, numFrames, 0, 1);
    }

    std::unique_ptr<VulkanImageView> VulkanRenderPass::createRenderTargetView(unsigned int index, unsigned int baseLayer, unsigned int numLayers) const
    {
        return std::make_unique<VulkanImageView>(m_renderer->getDevice(), *m_renderTargets.at(index), numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D, baseLayer, numLayers);
    }

    VkSampleCountFlagBits VulkanRenderPass::getDefaultSampleCount() const
    {
        return m_defaultSampleCount;
    }

    void VulkanRenderPass::freeResources()
    {
        m_renderTargets.clear();
        m_renderTargetViews.clear();
        m_framebuffers.clear();
    }
}
