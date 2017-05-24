#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"
#include "vulkan/FormatTraits.hpp"
#include "vulkan/VulkanDescriptorSetLayout.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanRenderPass;
    class VulkanDevice;

    class VulkanPipeline
    {
    public:
        VulkanPipeline(VulkanRenderer* renderer, uint32_t layoutCount, VulkanRenderPass* renderPass);
        ~VulkanPipeline();

        inline VkPipeline       getHandle()         const { return m_pipeline; }
        inline VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

        void resize(int width, int height);
        void bind(VkCommandBuffer buffer) const;
        
        VkDescriptorSet              allocateDescriptorSet(uint32_t setId) const;
        std::vector<VkDescriptorSet> allocateDescriptorSet(uint32_t setId, uint32_t count) const;

    protected:
        static VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState();
        static VkPipelineMultisampleStateCreateInfo   createDefaultMultisampleState();
        static VkPipelineColorBlendAttachmentState    createDefaultColorBlendAttachmentState();
        static VkPipelineColorBlendStateCreateInfo    createDefaultColorBlendState();
        static VkPipelineDepthStencilStateCreateInfo  createDefaultDepthStencilState();
    
        virtual void create(int width, int height) = 0;

        std::shared_ptr<VulkanDescriptorSetLayout> createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);
        VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxAllocatedSets);
        VkPipelineLayout createPipelineLayout(const std::vector<std::shared_ptr<VulkanDescriptorSetLayout>>& setLayouts, const std::vector<VkPushConstantRange>& pushConstants = std::vector<VkPushConstantRange>());

        VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char* entryPoint = "main");

        VulkanRenderer*   m_renderer;
        VulkanRenderPass* m_renderPass;
        VulkanDevice*     m_device;

        std::vector<std::shared_ptr<VulkanDescriptorSetLayout>> m_descriptorSetLayouts;
        VkDescriptorPool m_descriptorPool;

        VkPipeline       m_pipeline;
        VkPipelineLayout m_pipelineLayout;
    };
}