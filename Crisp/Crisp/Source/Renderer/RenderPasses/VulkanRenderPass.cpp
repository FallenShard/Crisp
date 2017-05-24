#include "VulkanRenderPass.hpp"

#include "vulkan/VulkanDevice.hpp"
#include "Renderer/VulkanRenderer.hpp"

namespace crisp
{
    VulkanRenderPass::VulkanRenderPass(VulkanRenderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice())
        , m_renderPass(VK_NULL_HANDLE)
    {
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
    }

    void VulkanRenderPass::recreate()
    {
        freeResources();
        createResources();
    }

    VkRenderPass VulkanRenderPass::getHandle() const
    {
        return m_renderPass;
    }

    VkExtent2D VulkanRenderPass::getRenderArea() const
    {
        return m_renderArea;
    }

    void VulkanRenderPass::end(VkCommandBuffer cmdBuffer) const
    {
        vkCmdEndRenderPass(cmdBuffer);
    }
}
