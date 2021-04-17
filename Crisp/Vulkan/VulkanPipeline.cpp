#include "Vulkan/VulkanPipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"

namespace crisp
{
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout, PipelineDynamicStateFlags dynamicStateFlags)
        : VulkanResource(device, pipelineHandle)
        , m_pipelineLayout(std::move(pipelineLayout))
        , m_dynamicStateFlags(dynamicStateFlags)
        , m_subpassIndex(0)
        , m_bindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
    }

    VulkanPipeline::~VulkanPipeline()
    {
        m_device->deferDestruction(m_framesToLive, m_handle, [](void* handle, VkDevice device)
        {
            spdlog::debug("Destroying pipeline: {}", handle);
            vkDestroyPipeline(device, static_cast<VkPipeline>(handle), nullptr);
        });
    }

    void VulkanPipeline::bind(VkCommandBuffer cmdBuffer) const
    {
        vkCmdBindPipeline(cmdBuffer, m_bindPoint, m_handle);
    }

    VulkanDescriptorSet VulkanPipeline::allocateDescriptorSet(uint32_t setId) const
    {
        return VulkanDescriptorSet(setId, m_pipelineLayout.get());
    }
}