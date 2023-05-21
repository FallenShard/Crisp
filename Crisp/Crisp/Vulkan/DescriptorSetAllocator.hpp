#pragma once

#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <memory>
#include <vector>

namespace crisp
{
class DescriptorSetAllocator
{
public:
    DescriptorSetAllocator(
        const VulkanDevice& device,
        const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings,
        const std::vector<uint32_t>& numCopiesPerSet,
        VkDescriptorPoolCreateFlags flags = 0);
    ~DescriptorSetAllocator();

    VkDescriptorSet allocate(
        VkDescriptorSetLayout setLayout, const std::vector<VkDescriptorSetLayoutBinding>& setBindings);

    const VulkanDevice& getDevice() const;

private:
    struct DescriptorPool
    {
        VkDescriptorPool handle;

        uint32_t currentAllocations;
        uint32_t maxAllocations;

        std::vector<VkDescriptorPoolSize> freeSizes;

        bool canAllocateSet(const std::vector<VkDescriptorSetLayoutBinding>& setBindings) const;
        void deductPoolSizes(const std::vector<VkDescriptorSetLayoutBinding>& setBindings);

        VkDescriptorSet allocate(
            VkDevice device,
            VkDescriptorSetLayout setLayout,
            const std::vector<VkDescriptorSetLayoutBinding>& setBindings);
    };

    const VulkanDevice* m_device;
    std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
};
} // namespace crisp