#include "ComputePipeline.hpp"

#include "Renderer/Renderer.hpp"
#include "vulkan/VulkanDevice.hpp"

namespace crisp
{
    ComputePipeline::ComputePipeline(Renderer* renderer, std::string&& shaderName, uint32_t numDynamicStorageBuffers, uint32_t numDescriptorSets, std::size_t pushConstantSize, const glm::uvec3& workGroupSize)
        : VulkanPipeline(renderer, 1, nullptr, false)
        , m_workGroupSize(workGroupSize)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (uint32_t i = 0; i < numDynamicStorageBuffers; i++)
            bindings.push_back({ i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr });
        m_descriptorSetLayouts[0] = createDescriptorSetLayout(bindings);

        m_pipelineLayout = createPipelineLayout(m_descriptorSetLayouts,
        {
            { VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<uint32_t>(pushConstantSize) }
        });

        m_descriptorPool = createDescriptorPool(
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numDescriptorSets * numDynamicStorageBuffers }
        }, numDescriptorSets);

        m_shader = renderer->getShaderModule(std::forward<std::string>(shaderName));

        create(renderer->getSwapChainExtent().width, renderer->getSwapChainExtent().height);

        std::vector<VkSpecializationMapEntry> specEntries =
        {
            { 0, 0 * sizeof(uint32_t), sizeof(uint32_t) },
            { 1, 1 * sizeof(uint32_t), sizeof(uint32_t) },
            { 2, 2 * sizeof(uint32_t), sizeof(uint32_t) }
        };

        VkSpecializationInfo specInfo = {};
        specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
        specInfo.pMapEntries   = specEntries.data();
        specInfo.dataSize      = sizeof(m_workGroupSize);
        specInfo.pData         = glm::value_ptr(m_workGroupSize);

        VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage                     = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, m_shader);
        pipelineInfo.stage.pSpecializationInfo = &specInfo;
        pipelineInfo.layout                    = m_pipelineLayout;
        pipelineInfo.basePipelineHandle        = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex         = -1;
        vkCreateComputePipelines(m_device->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle);
    }

    const glm::uvec3& ComputePipeline::getWorkGroupSize() const
    {
        return m_workGroupSize;
    }

    void ComputePipeline::create(int, int)
    {
    }
}