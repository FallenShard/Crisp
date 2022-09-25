#pragma once

#include <Crisp/Vulkan/VulkanResource.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class VulkanDevice;
class DescriptorSetAllocator;

struct DescriptorSetLayout
{
    VkDescriptorSetLayout handle;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<uint32_t> dynamicBufferIndices;
    bool isBuffered{false};
};

class VulkanPipelineLayout : public VulkanResource<VkPipelineLayout, vkDestroyPipelineLayout>
{
public:
    VulkanPipelineLayout(
        const VulkanDevice& device,
        std::vector<VkDescriptorSetLayout>&& setLayouts,
        std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings,
        std::vector<VkPushConstantRange>&& pushConstants,
        std::vector<bool> descriptorSetBufferedStatus,
        std::unique_ptr<DescriptorSetAllocator> setAllocator);
    virtual ~VulkanPipelineLayout();

    inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const
    {
        return m_descriptorSetLayouts.at(setIndex).bindings.at(binding).descriptorType;
    }

    VkDescriptorSet allocateSet(uint32_t setIndex) const;

    inline void setPushConstants(VkCommandBuffer cmdBuffer, const char* data) const
    {
        for (const auto& pushConstant : m_pushConstants)
            vkCmdPushConstants(
                cmdBuffer,
                m_handle,
                pushConstant.stageFlags,
                pushConstant.offset,
                pushConstant.size,
                data + pushConstant.offset);
    }

    inline std::size_t getDescriptorSetLayoutCount() const
    {
        return m_descriptorSetLayouts.size();
    }

    inline VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const
    {
        return m_descriptorSetLayouts.at(setIndex).handle;
    }

    inline const std::vector<VkDescriptorSetLayoutBinding>& getDescriptorSetLayoutBindings(uint32_t setIndex) const
    {
        return m_descriptorSetLayouts.at(setIndex).bindings;
    }

    inline bool isDescriptorSetBuffered(uint32_t setIndex) const
    {
        return m_descriptorSetLayouts.at(setIndex).isBuffered;
    }

    inline std::size_t getDynamicBufferCount() const
    {
        return m_dynamicBufferCount;
    }

    inline uint32_t getDynamicBufferIndex(uint32_t setIndex, uint32_t binding) const
    {
        return m_descriptorSetLayouts.at(setIndex).dynamicBufferIndices.at(binding);
    }

    inline DescriptorSetAllocator* getDescriptorSetAllocator()
    {
        return m_setAllocator.get();
    }

    void swap(VulkanPipelineLayout& other);

    std::unique_ptr<DescriptorSetAllocator> createDescriptorSetAllocator(
        VulkanDevice& device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0);

private:
    std::vector<DescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<VkPushConstantRange> m_pushConstants;

    uint32_t m_dynamicBufferCount;

    std::unique_ptr<DescriptorSetAllocator> m_setAllocator;
};
} // namespace crisp
