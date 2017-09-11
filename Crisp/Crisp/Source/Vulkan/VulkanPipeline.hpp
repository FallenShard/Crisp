#pragma once

#include <vector>
#include <memory>

#include "VulkanResource.hpp"

#include "Math/Headers.hpp"
#include "vulkan/VulkanFormatTraits.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"
#include "vulkan/VulkanDescriptorSet.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanRenderPass;
    class VulkanDevice;

    class VulkanPipeline : public VulkanResource<VkPipeline>
    {
    public:
        VulkanPipeline(VulkanRenderer* renderer, uint32_t layoutCount, VulkanRenderPass* renderPass);
        ~VulkanPipeline();

        inline VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
        inline VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }

        void resize(int width, int height);
        void bind(VkCommandBuffer buffer) const;
        void bind(VkCommandBuffer buffer, VkPipelineBindPoint bindPoint) const;

        VulkanDescriptorSet          allocateDescriptorSet(uint32_t setId) const;
        VulkanDescriptorSetLayout*   getDescriptorSetLayout(uint32_t index) const;

        template <typename T>
        inline void setPushConstant(VkCommandBuffer cmdBuffer, VkShaderStageFlags shaderStages, uint32_t offset, const T& value) const
        {
            vkCmdPushConstants(cmdBuffer, m_pipelineLayout, shaderStages, offset, sizeof(T), &value);
        }

    protected:
        static VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState();
        static VkPipelineMultisampleStateCreateInfo   createDefaultMultisampleState();
        static VkPipelineColorBlendAttachmentState    createDefaultColorBlendAttachmentState();
        static VkPipelineColorBlendStateCreateInfo    createDefaultColorBlendState();
        static VkPipelineDepthStencilStateCreateInfo  createDefaultDepthStencilState();
    
        virtual void create(int width, int height) = 0;

        std::unique_ptr<VulkanDescriptorSetLayout> createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxAllocatedSets);
        VkPipelineLayout createPipelineLayout(const std::vector<std::unique_ptr<VulkanDescriptorSetLayout>>& setLayouts, const std::vector<VkPushConstantRange>& pushConstants = std::vector<VkPushConstantRange>());

        VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");

        VulkanRenderer*   m_renderer;
        VulkanRenderPass* m_renderPass;

        std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> m_descriptorSetLayouts;
        VkDescriptorPool m_descriptorPool;
        VkPipelineLayout m_pipelineLayout;
    };
}