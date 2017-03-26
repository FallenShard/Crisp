#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include "Math/Headers.hpp"

namespace crisp
{
    class VulkanRenderer;
    class VulkanRenderPass;

    class VulkanPipeline
    {
    public:
        VulkanPipeline(VulkanRenderer* renderer, uint32_t layoutCount, VulkanRenderPass* renderPass);
        ~VulkanPipeline();

        inline VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
        inline VkDescriptorSetLayout getDescriptorSetLayout(uint32_t setId) const { return m_descriptorSetLayouts.at(setId); }
        inline VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
        inline VkPipeline getHandle() const { return m_pipeline; }

        void resize(int width, int height);
        VkDescriptorSet allocateDescriptorSet(uint32_t setId) const;
        std::vector<VkDescriptorSet> allocateDescriptorSet(uint32_t setId, uint32_t count) const;

        static VkPipelineRasterizationStateCreateInfo createDefaultRasterizationState();
        static VkPipelineMultisampleStateCreateInfo createDefaultMultisampleState();
        static VkPipelineColorBlendAttachmentState createDefaultColorBlendAttachmentState();
        static VkPipelineColorBlendStateCreateInfo createDefaultColorBlendState();
        static VkPipelineDepthStencilStateCreateInfo createDefaultDepthStencilState();

    protected:
        virtual void create(int width, int height) = 0;

        void createDescriptorSetLayout(size_t index, const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayoutCreateFlags flags = 0);

        VulkanRenderer* m_renderer;
        VulkanRenderPass* m_renderPass;

        VkDevice m_device;

        std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
        VkDescriptorPool m_descriptorPool;

        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_pipeline;
    };
}