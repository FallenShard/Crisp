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

    void VulkanRenderPass::end(VkCommandBuffer cmdBuffer) const
    {
        vkCmdEndRenderPass(cmdBuffer);
    }

    void VulkanRenderPass::nextSubpass(VkCommandBuffer cmdBuffer, VkSubpassContents contents) const
    {
        vkCmdNextSubpass(cmdBuffer, contents);
    }
}
