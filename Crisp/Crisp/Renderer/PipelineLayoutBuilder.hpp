#pragma once

#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/Vulkan/VulkanDescriptorSetAllocator.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>
#include <Crisp/Vulkan/VulkanPipelineLayout.hpp>

namespace crisp {
class PipelineLayoutBuilder {
public:
    PipelineLayoutBuilder() = default;
    explicit PipelineLayoutBuilder(PipelineLayoutMetadata&& metadata);

    PipelineLayoutBuilder& defineDescriptorSet(
        uint32_t set,
        bool isBuffered,
        std::vector<VkDescriptorSetLayoutBinding>&& bindings,
        VkDescriptorSetLayoutCreateFlags flags = 0);
    PipelineLayoutBuilder& addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

    std::vector<VkDescriptorSetLayout> createDescriptorSetLayoutHandles(VkDevice device) const;
    std::unique_ptr<VulkanPipelineLayout> create(
        const VulkanDevice& device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0) const;

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> getDescriptorSetLayoutBindings() const;
    std::vector<bool> getDescriptorSetBufferedStatuses() const;
    std::vector<VkPushConstantRange> getPushConstantRanges() const;

    std::size_t getDescriptorSetLayoutCount() const;

    std::vector<std::vector<uint32_t>> getBindlessBindings() const;

    void setDescriptorSetBuffering(uint32_t setIndex, bool isBuffered);
    void setDescriptorDynamic(uint32_t setIndex, uint32_t binding, bool isDynamic);
    void setDescriptorBindless(uint32_t setIndex, uint32_t binding, uint32_t maxDescriptorCount);

private:
    PipelineLayoutMetadata m_metadata;

    std::vector<VkDescriptorSetLayoutCreateFlags> m_createFlags;
    std::vector<bool> m_setBindless;
    std::vector<bool> m_setBuffered;
    std::vector<std::vector<uint32_t>> m_bindlessBindings;
};
} // namespace crisp