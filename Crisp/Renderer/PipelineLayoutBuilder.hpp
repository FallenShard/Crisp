#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;
    class VulkanPipelineLayout;
    class DescriptorSetAllocator;

    namespace sl
    {
        class Reflection;
    }

    class PipelineLayoutBuilder
    {
    public:
        PipelineLayoutBuilder() {}
        PipelineLayoutBuilder(const sl::Reflection& shaderReflection);

        PipelineLayoutBuilder& defineDescriptorSet(uint32_t set, std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        PipelineLayoutBuilder& defineDescriptorSet(uint32_t set, bool isBuffered, std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        PipelineLayoutBuilder& addPushConstant(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

        std::vector<VkDescriptorSetLayout> createDescriptorSetLayouts(VkDevice device) const;
        VkPipelineLayout createHandle(VkDevice device, VkDescriptorSetLayout* setLayouts, uint32_t setLayoutCount);
        std::unique_ptr<VulkanPipelineLayout> create(VulkanDevice* device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0) const;
        std::unique_ptr<DescriptorSetAllocator> createMinimalDescriptorSetAllocator(VulkanDevice* device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0) const;

        std::vector<std::vector<VkDescriptorSetLayoutBinding>> getDescriptorSetLayoutBindings() const;
        std::vector<bool> getDescriptorSetBufferedStatuses() const;
        std::vector<VkPushConstantRange> getPushConstantRanges() const;

        std::size_t getDescriptorSetLayoutCount() const;


        void setDescriptorSetBuffering(int index, bool isBuffered);
        void setDescriptorDynamic(int setIndex, int binding, bool isDynamic);

    private:
        std::vector<uint32_t> getNumCopiesPerSet(uint32_t numCopies) const;

        std::vector<VkDescriptorSetLayoutCreateInfo> m_setLayoutCreateInfos;

        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_setLayoutBindings;
        std::vector<bool>                                      m_setBuffered;
        std::vector<VkPushConstantRange>                       m_pushConstantRanges;
    };
}