#pragma once

#include <Crisp/Vulkan/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanPipelineLayout.hpp>

namespace crisp {
class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(uint32_t index, const VulkanPipelineLayout* pipelineLayout);

    inline VkDescriptorSet getHandle() const {
        return m_set;
    }

    VkDescriptorType getDescriptorType(uint32_t index) const;

private:
    VkDescriptorSet m_set;
    uint32_t m_index;
    const VulkanPipelineLayout* m_pipelinelayout;
};
} // namespace crisp