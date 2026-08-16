#include <Crisp/Renderer/ComputePipeline.hpp>

#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>

#include <Crisp/Io/FileUtils.hpp>
#include <Crisp/Vulkan/PipelineBuilder.hpp>
#include <Crisp/Vulkan/PipelineLayoutBuilder.hpp>
#include <Crisp/Vulkan/Rhi/VulkanDevice.hpp>

#include <Crisp/ShaderUtils/Reflection.hpp>

#include <vector>

namespace crisp {
FlatHashMap<VkPipeline, VkExtent3D> workGroupSizes;

VkExtent3D getWorkGroupSize(const VulkanPipeline& pipeline) {
    return workGroupSizes.at(pipeline.getHandle());
}

namespace {
std::unique_ptr<VulkanPipeline> createComputePipelineFromModule(
    const VulkanDevice& device,
    VkShaderModule shaderModule,
    std::unique_ptr<VulkanPipelineLayout> layout,
    const VkExtent3D& workGroupSize,
    const std::span<const uint32_t> specializationConstants) {
    std::vector<uint32_t> specializationData{
        workGroupSize.width,
        workGroupSize.height,
        workGroupSize.depth,
    };
    specializationData.insert(specializationData.end(), specializationConstants.begin(), specializationConstants.end());

    // IDs 0-2 are reserved by the shaders for local_size_*_id; caller-provided values follow them.
    std::vector<VkSpecializationMapEntry> specEntries(specializationData.size());
    for (uint32_t i = 0; i < specEntries.size(); ++i) {
        specEntries[i] = VkSpecializationMapEntry{i, i * sizeof(uint32_t), sizeof(uint32_t)};
    }

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = specializationData.size() * sizeof(specializationData[0]);
    specInfo.pData = specializationData.data();

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT, shaderModule);
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline{VK_NULL_HANDLE};
    VK_FATAL(vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    workGroupSizes[pipeline] = workGroupSize;

    return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE);
}
} // namespace

std::unique_ptr<VulkanPipeline> createComputePipeline(
    const VulkanDevice& device,
    const std::filesystem::path& spvPath,
    const VkExtent3D& workGroupSize,
    const std::function<void(PipelineLayoutBuilder&)>& builderOverride,
    const std::span<const uint32_t> specializationConstants) {
    PipelineLayoutBuilder layoutBuilder(reflectPipelineLayoutFromSpirv(spvPath).unwrap());
    if (builderOverride) {
        builderOverride(layoutBuilder);
    }
    auto layout = layoutBuilder.create(device);

    const auto shaderCode = readBinaryFile(spvPath).unwrap();
    VkShaderModuleCreateInfo moduleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = shaderCode.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()); // NOLINT
    VkShaderModule shaderModule{VK_NULL_HANDLE};
    VK_FATAL(vkCreateShaderModule(device.getHandle(), &moduleInfo, nullptr, &shaderModule));

    auto pipeline = createComputePipelineFromModule(
        device, shaderModule, std::move(layout), workGroupSize, specializationConstants);
    vkDestroyShaderModule(device.getHandle(), shaderModule, nullptr);
    return pipeline;
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
