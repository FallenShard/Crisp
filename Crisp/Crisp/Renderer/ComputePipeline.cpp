#include "ComputePipeline.hpp"

#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/VulkanDevice.hpp>

#include <Crisp/ShaderUtils/Reflection.hpp>

namespace crisp {
FlatHashMap<VkPipeline, glm::uvec3> workGroupSizes;

glm::uvec3 getWorkGroupSize(const VulkanPipeline& pipeline) {
    return workGroupSizes.at(pipeline.getHandle());
}

std::unique_ptr<VulkanPipeline> createComputePipeline(
    const VulkanDevice& device, VkPipeline pipelineHandle, std::unique_ptr<VulkanPipelineLayout> pipelineLayout) {
    return std::make_unique<VulkanPipeline>(
        device, pipelineHandle, std::move(pipelineLayout), VK_PIPELINE_BIND_POINT_COMPUTE);
}

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer* renderer,
    std::string&& shaderName,
    uint32_t numDynamicStorageBuffers,
    uint32_t /*numDescriptorSets*/,
    const glm::uvec3& workGroupSize) {
    auto refl =
        reflectUniformMetadataFromSpirvPath(renderer->getAssetPaths().spvShaderDir / (shaderName + ".spv")).unwrap();

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (uint32_t i = 0; i < numDynamicStorageBuffers; i++) {
        bindings.push_back({i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT});
    }

    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder.defineDescriptorSet(0, false, std::move(bindings));
    if (!refl.pushConstants.empty()) {
        layoutBuilder.addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, refl.pushConstants.front().size);
    }
    auto layout = layoutBuilder.create(renderer->getDevice());

    std::vector<VkSpecializationMapEntry> specEntries = {
        {0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 2 * sizeof(uint32_t), sizeof(uint32_t)}};

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = glm::value_ptr(workGroupSize);

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(
        VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule(std::forward<std::string>(shaderName)));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipelineHandle;
    vkCreateComputePipelines(
        renderer->getDevice().getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineHandle);
    workGroupSizes[pipelineHandle] = workGroupSize;

    return createComputePipeline(renderer->getDevice(), pipelineHandle, std::move(layout));
}

std::unique_ptr<VulkanPipeline> createLightCullingComputePipeline(Renderer* renderer, const glm::uvec3& workGroupSize) {
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder
        .defineDescriptorSet(
            0,
            false,
            {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
             {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
             {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
             {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
             {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT}})
        .defineDescriptorSet(
            1,
            true,
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            });

    VulkanDevice& device = renderer->getDevice();
    auto layout = layoutBuilder.create(device);

    std::vector<VkSpecializationMapEntry> specEntries = {
        //   id,               offset,             size
        {0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 2 * sizeof(uint32_t), sizeof(uint32_t)}};

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = glm::value_ptr(workGroupSize);

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage =
        createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer->getShaderModule("light-culling.comp"));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline;
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    workGroupSizes[pipeline] = workGroupSize;

    return createComputePipeline(renderer->getDevice(), pipeline, std::move(layout));
}

glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline) {
    return (dataDims - glm::uvec3(1)) / getWorkGroupSize(pipeline) + glm::uvec3(1);
}

glm::uvec3 computeWorkGroupCount(const glm::uvec3& dataDims, const glm::uvec3& workGroupSize) {
    return (dataDims - glm::uvec3(1)) / workGroupSize + glm::uvec3(1);
}

} // namespace crisp