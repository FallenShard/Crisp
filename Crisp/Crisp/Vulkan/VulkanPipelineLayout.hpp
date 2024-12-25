#pragma once

#include <memory>
#include <vector>

#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>
#include <Crisp/Vulkan/Rhi/VulkanResource.hpp>
#include <Crisp/Vulkan/VulkanDescriptorSetAllocator.hpp>

namespace crisp {
struct DescriptorSetLayout {
    VkDescriptorSetLayout handle;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<uint32_t> dynamicBufferIndices;
    uint32_t dynamicBufferCount{0};

    std::vector<uint32_t> bindlessIndices;

    bool isBuffered{false};
};

class VulkanPipelineLayout : public VulkanResource<VkPipelineLayout> {
public:
    VulkanPipelineLayout(
        const VulkanDevice& device,
        std::vector<VkDescriptorSetLayout>&& setLayouts,
        std::vector<std::vector<VkDescriptorSetLayoutBinding>>&& setBindings,
        std::vector<VkPushConstantRange>&& pushConstants,
        std::vector<bool> descriptorSetBufferedStatus,
        std::vector<std::vector<uint32_t>> bindlessIndices,
        std::unique_ptr<VulkanDescriptorSetAllocator> setAllocator);
    ~VulkanPipelineLayout() override;

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;

    VulkanPipelineLayout(VulkanPipelineLayout&&) noexcept = default;
    VulkanPipelineLayout& operator=(VulkanPipelineLayout&&) noexcept = default;

    inline VkDescriptorType getDescriptorType(uint32_t setIndex, uint32_t binding) const {
        return m_descriptorSetLayouts.at(setIndex).bindings.at(binding).descriptorType;
    }

    VkDescriptorSet allocateSet(uint32_t setIndex) const;

    inline void setPushConstants(VkCommandBuffer cmdBuffer, const char* data) const {
        for (const auto& pushConstant : m_pushConstants) {
            vkCmdPushConstants(
                cmdBuffer,
                m_handle,
                pushConstant.stageFlags,
                pushConstant.offset,
                pushConstant.size,
                data + pushConstant.offset); // NOLINT
        }
    }

    inline uint32_t getDescriptorSetLayoutCount() const {
        return static_cast<uint32_t>(m_descriptorSetLayouts.size());
    }

    inline VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setIndex) const {
        return m_descriptorSetLayouts.at(setIndex).handle;
    }

    inline const std::vector<VkDescriptorSetLayoutBinding>& getDescriptorSetLayoutBindings(uint32_t setIndex) const {
        return m_descriptorSetLayouts.at(setIndex).bindings;
    }

    inline const std::vector<uint32_t>& getDescriptorSetBindlessBindings(uint32_t setIndex) const {
        return m_descriptorSetLayouts.at(setIndex).bindlessIndices;
    }

    inline bool isDescriptorSetBuffered(uint32_t setIndex) const {
        return m_descriptorSetLayouts.at(setIndex).isBuffered;
    }

    inline uint32_t getDynamicBufferCount() const {
        return m_dynamicBufferCount;
    }

    inline uint32_t getDynamicBufferCount(const uint32_t setIndex) const {
        return static_cast<uint32_t>(m_descriptorSetLayouts.at(setIndex).dynamicBufferCount);
    }

    inline uint32_t getDynamicBufferIndex(const uint32_t setIndex, const uint32_t binding) const {
        return m_descriptorSetLayouts.at(setIndex).dynamicBufferIndices.at(binding);
    }

    inline VulkanDescriptorSetAllocator* getVulkanDescriptorSetAllocator() {
        return m_setAllocator.get();
    }

    void swap(VulkanPipelineLayout& other) noexcept;

    std::unique_ptr<VulkanDescriptorSetAllocator> createVulkanDescriptorSetAllocator(
        VulkanDevice& device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0);

private:
    std::vector<DescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<VkPushConstantRange> m_pushConstants;

    uint32_t m_dynamicBufferCount;

    std::unique_ptr<VulkanDescriptorSetAllocator> m_setAllocator;
};

std::vector<uint32_t> computeCopiesPerSet(const std::vector<bool>& isSetBuffered, uint32_t numCopies);

} // namespace crisp
