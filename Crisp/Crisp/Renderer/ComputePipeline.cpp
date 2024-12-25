#include <Crisp/Renderer/ComputePipeline.hpp>

#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

#include <Crisp/ShaderUtils/Reflection.hpp>

namespace crisp {
FlatHashMap<VkPipeline, VkExtent3D> workGroupSizes;

VkExtent3D getWorkGroupSize(const VulkanPipeline& pipeline) {
    return workGroupSizes.at(pipeline.getHandle());
}

std::unique_ptr<VulkanPipeline> createComputePipeline(
    Renderer& renderer,
    const std::string& shaderName,
    const VkExtent3D& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride) {
    const VulkanDevice& device = renderer.getDevice();
    PipelineLayoutBuilder layoutBuilder(
        reflectPipelineLayoutFromSpirvPath(renderer.getAssetPaths().spvShaderDir / (shaderName + ".spv")).unwrap());
    if (builderOverride) {
        builderOverride(layoutBuilder);
    }
    auto layout = layoutBuilder.create(device);

    const std::array<VkSpecializationMapEntry, 3> specEntries = {
        VkSpecializationMapEntry{0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        VkSpecializationMapEntry{1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        VkSpecializationMapEntry{2, 2 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = &workGroupSize;

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, renderer.getOrLoadShaderModule(shaderName));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline{VK_NULL_HANDLE};
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    workGroupSizes[pipeline] = workGroupSize;

    return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE);
}

VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VulkanPipeline& pipeline) {
    const auto workGroupSize = getWorkGroupSize(pipeline);
    return {
        .width = (dataDims.x + workGroupSize.width - 1) / workGroupSize.width,
        .height = (dataDims.y + workGroupSize.height - 1) / workGroupSize.height,
        .depth = (dataDims.z + workGroupSize.depth - 1) / workGroupSize.depth,
    };
}

VkExtent3D computeWorkGroupCount(const glm::uvec3& dataDims, const VkExtent3D& workGroupSize) {
    return {
        .width = (dataDims.x + workGroupSize.width - 1) / workGroupSize.width,
        .height = (dataDims.y + workGroupSize.height - 1) / workGroupSize.height,
        .depth = (dataDims.z + workGroupSize.depth - 1) / workGroupSize.depth,
    };
}

} // namespace crisp