#pragma once

#include <memory>
#include <vector>

#include <Crisp/Vulkan/VulkanDevice.hpp>

namespace crisp {
class VulkanDescriptorSetAllocator {
public:
    VulkanDescriptorSetAllocator(
        const VulkanDevice& device,
        const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings,
        const std::vector<uint32_t>& numCopiesPerSet,
        VkDescriptorPoolCreateFlags flags = 0);
    ~VulkanDescriptorSetAllocator();

    VulkanDescriptorSetAllocator(const VulkanDescriptorSetAllocator&) = delete;
    VulkanDescriptorSetAllocator& operator=(const VulkanDescriptorSetAllocator&) = delete;

    VulkanDescriptorSetAllocator(VulkanDescriptorSetAllocator&&) noexcept = default;
    VulkanDescriptorSetAllocator& operator=(VulkanDescriptorSetAllocator&&) noexcept = delete;

    VkDescriptorSet allocate(
        VkDescriptorSetLayout setLayout,
        const std::vector<VkDescriptorSetLayoutBinding>& setBindings,
        const std::vector<uint32_t>& bindlessBindings);

    const VulkanDevice& getDevice() const;

private:
    struct DescriptorPool {
        VkDescriptorPool handle;

        uint32_t currentAllocations;
        uint32_t maxAllocations;

        std::vector<VkDescriptorPoolSize> freeSizes;

        bool canAllocateSet(const std::vector<VkDescriptorSetLayoutBinding>& setBindings) const;
        void deductPoolSizes(const std::vector<VkDescriptorSetLayoutBinding>& setBindings);

        VkDescriptorSet allocate(
            VkDevice device,
            VkDescriptorSetLayout setLayout,
            const std::vector<VkDescriptorSetLayoutBinding>& setBindings,
            const std::vector<uint32_t>& bindlessBindings);
    };

    const VulkanDevice* m_device;
    std::vector<std::unique_ptr<DescriptorPool>> m_descriptorPools;
    const VkDescriptorPoolCreateFlags m_createFlags;
};
} // namespace crisp