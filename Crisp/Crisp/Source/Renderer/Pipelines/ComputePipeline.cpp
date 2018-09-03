#include "ComputePipeline.hpp"

#include "Vulkan/VulkanDevice.hpp"
#include "Renderer/PipelineLayoutBuilder.hpp"
#include "Renderer/PipelineBuilder.hpp"
#include "Renderer/Renderer.hpp"

namespace crisp
{
    std::unordered_map<VkPipeline, glm::uvec3> workGroupSizes;

    glm::uvec3 getWorkGroupSize(const VulkanPipeline& pipeline)
    {
        return workGroupSizes.at(pipeline.getHandle());
    }

    std::unique_ptr<VulkanPipeline> createComputePipeline(Renderer* renderer, std::string&& shaderName, uint32_t numDynamicStorageBuffers, uint32_t numDescriptorSets, std::size_t pushConstantSize, const glm::uvec3& workGroupSize)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (uint32_t i = 0; i < numDynamicStorageBuffers; i++)
            bindings.push_back({ i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT });

        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.defineDescriptorSet(0, std::move(bindings))
            .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<uint32_t>(pushConstantSize));

        VulkanDevice* device = renderer->getDevice();

        auto descPool = createDescriptorPool(device->getHandle(), layoutBuilder, { numDescriptorSets }, numDescriptorSets);
        auto layout   = createPipelineLayout(device, layoutBuilder, descPool);


        std::vector<VkSpecializationMapEntry> specEntries =
        {
            { 0, 0 * sizeof(uint32_t), sizeof(uint32_t) },
            { 1, 1 * sizeof(uint32_t), sizeof(uint32_t) },
            { 2, 2 * sizeof(uint32_t), sizeof(uint32_t) }
        };

        VkSpecializationInfo specInfo = {};
        specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
        specInfo.pMapEntries   = specEntries.data();
        specInfo.dataSize      = sizeof(workGroupSize);
        specInfo.pData         = glm::value_ptr(workGroupSize);

        VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage                     = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule(std::forward<std::string>(shaderName)));
        pipelineInfo.stage.pSpecializationInfo = &specInfo;
        pipelineInfo.layout                    = layout->getHandle();
        pipelineInfo.basePipelineHandle        = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex         = -1;
        VkPipeline pipeline;
        vkCreateComputePipelines(device->getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
        workGroupSizes[pipeline] = workGroupSize;

        return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), PipelineDynamicStateFlags());
    }
}