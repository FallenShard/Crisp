#include "VulkanRenderPass.hpp"

#include "vulkan/VulkanDevice.hpp"
#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    VulkanRenderPass::VulkanRenderPass(VulkanRenderer* renderer)
        : VulkanResource(renderer->getDevice())
        , m_renderer(renderer)
    {
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        vkDestroyRenderPass(m_device->getHandle(), m_handle, nullptr);
    }

    void VulkanRenderPass::recreate()
    {
        freeResources();
        createResources();
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

    void VulkanRenderPass::end(VkCommandBuffer cmdBuffer) const
    {
        vkCmdEndRenderPass(cmdBuffer);
    }

    void VulkanRenderPass::nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents) const
    {
        vkCmdNextSubpass(cmdBuffer, contents);
    }
}
