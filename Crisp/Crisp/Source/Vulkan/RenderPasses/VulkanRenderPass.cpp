#include "VulkanRenderPass.hpp"

#include "Vulkan/VulkanRenderer.hpp"

namespace crisp
{
    VulkanRenderPass::VulkanRenderPass(VulkanRenderer* renderer)
        : m_renderer(renderer)
        , m_device(renderer->getDevice().getHandle())
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

    void VulkanRenderPass::end(VkCommandBuffer cmdBuffer) const
    {
        vkCmdEndRenderPass(cmdBuffer);
    }
}
