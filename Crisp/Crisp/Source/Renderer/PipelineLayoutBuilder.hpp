#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

namespace crisp
{
    class VulkanDevice;
    class VulkanPipelineLayout;

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

        VkPipelineLayout create(VkDevice device);
        std::unique_ptr<VulkanPipelineLayout> create(VulkanDevice* device, VkDescriptorPool descriptorPool);
        std::unique_ptr<VulkanPipelineLayout> create(VulkanDevice* device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0);
        VkDescriptorPool createMinimalDescriptorPool(VulkanDevice* device, uint32_t numCopies = 1, VkDescriptorPoolCreateFlags flags = 0) const;

        std::vector<VkDescriptorSetLayout> extractDescriptorSetLayouts();
        std::vector<std::vector<VkDescriptorSetLayoutBinding>> extractDescriptorSetBindings();
        std::vector<VkPushConstantRange> extractPushConstants();

        const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& getDescriptorSetBindings() const;

    private:
        std::vector<uint32_t> getNumCopiesPerSet(uint32_t numCopies) const;

        std::vector<std::vector<VkDescriptorSetLayoutBinding>> m_setBindings;
        std::vector<VkDescriptorSetLayoutCreateInfo>           m_setCreateInfos;
        std::vector<VkDescriptorSetLayout>                     m_setLayouts;
        std::vector<VkPushConstantRange>                       m_pushConstants;
        std::vector<bool>                                      m_setBuffered;
    };

    std::vector<VkDescriptorPoolSize> calculateMinimumPoolSizes(const std::vector<std::vector<VkDescriptorSetLayoutBinding>>& setBindings, const std::vector<uint32_t>& numCopies = {});
    VkDescriptorPool createDescriptorPool(VkDevice device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSetCount, VkDescriptorPoolCreateFlags flags = 0);
    VkDescriptorPool createDescriptorPool(VkDevice device, const PipelineLayoutBuilder& builder, const std::vector<uint32_t>& numCopies, uint32_t maxSetCount, VkDescriptorPoolCreateFlags flags = 0);
}