#pragma once

#include <memory>
#include <vector>

#include "VulkanResource.hpp"

namespace crisp
{
    class VulkanImage;
    class VulkanImageView;
    class VulkanFramebuffer;
    class Renderer;

    class VulkanRenderPass : public VulkanResource<VkRenderPass>
    {
    public:
        VulkanRenderPass(Renderer* renderer, bool isWindowSizeDependent, uint32_t numSubpasses);
        virtual ~VulkanRenderPass();
        void recreate();

        VkExtent2D getRenderArea() const;
        VkViewport createViewport() const;
        VkRect2D createScissor() const;

        void begin(VkCommandBuffer cmdBuffer) const;
        void begin(VkCommandBuffer cmdBuffer, VkSubpassContents content) const;
        void begin(VkCommandBuffer cmdBuffer, uint32_t frameIndex) const;
        void end(VkCommandBuffer cmdBuffer, uint32_t frameIndex) const;
        void nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) const;

        VulkanImage*     getRenderTarget(unsigned int index) const;
        const VulkanImageView& getRenderTargetView(unsigned int renderTargetIndex, unsigned int frameIndex) const;
        std::vector<VulkanImageView*> getRenderTargetViews(unsigned int renderTargetIndex) const;
        std::unique_ptr<VulkanImageView> createRenderTargetView(unsigned int index, unsigned int numFrames) const;
        std::unique_ptr<VulkanImageView> createRenderTargetView(unsigned int index, unsigned int baseLayer, unsigned int numLayers) const;

        inline uint32_t getNumSubpasses() const { return m_numSubpasses; }

        VkSampleCountFlagBits getDefaultSampleCount() const;

        const VulkanFramebuffer* getFramebuffer(uint32_t frameIdx) const { return m_framebuffers.at(frameIdx).get(); }

    protected:
        virtual void createResources() = 0;
        void freeResources();

        Renderer* m_renderer;

        uint32_t m_numSubpasses;
        bool m_isWindowSizeDependent;
        VkExtent2D m_renderArea;
        VkSampleCountFlagBits m_defaultSampleCount;

        std::vector<VkImageLayout> m_finalLayouts;
        std::vector<std::unique_ptr<VulkanImage>> m_renderTargets;
        std::vector<std::vector<std::unique_ptr<VulkanImageView>>> m_renderTargetViews;
        std::vector<VkClearValue> m_clearValues;

        std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;
    };
}
