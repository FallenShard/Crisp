#include <Crisp/Vulkan/RayTracingPipelineBuilder.hpp>

#include <cstring>
#include <span>

#include <Crisp/ShaderUtils/Reflection.hpp>
#include <Crisp/ShaderUtils/ShaderType.hpp>
#include <Crisp/Vulkan/Rhi/VulkanChecks.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>

namespace crisp {
RayTracingPipelineBuilder::RayTracingPipelineBuilder(VulkanDevice& device)
    : m_device(device) {}

RayTracingPipelineBuilder::~RayTracingPipelineBuilder() {
    for (const auto module : m_shaderModules) {
        vkDestroyShaderModule(m_device.getHandle(), module, nullptr);
    }
}

void RayTracingPipelineBuilder::addShaderStage(const std::filesystem::path& spvPath) {
    const auto shaderCode = readSpirvFile(spvPath).unwrap();
    const VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderCode.size(),
        .pCode = reinterpret_cast<const uint32_t*>(shaderCode.data()), // NOLINT
    };
    VkShaderModule shaderModule{VK_NULL_HANDLE};
    VK_FATAL(vkCreateShaderModule(m_device.getHandle(), &createInfo, nullptr, &shaderModule));
    m_device.setObjectName(shaderModule, spvPath.stem().string());
    m_shaderModules.push_back(shaderModule);

    m_stages.push_back({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = getShaderStageFromFilePath(spvPath.stem()).unwrap(),
        .module = shaderModule,
        .pName = "main",
    });
    m_stageCounts[m_stages.back().stage]++;
}

void RayTracingPipelineBuilder::addShaderGroup(
    const uint32_t shaderStageIdx, const VkRayTracingShaderGroupTypeKHR type) {
    m_groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = type,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    });
    switch (type) {
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR: {
        m_groups.back().generalShader = shaderStageIdx;
        break;
    }
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR: {
        m_groups.back().closestHitShader = shaderStageIdx;
        break;
    }
    case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR: {
        m_groups.back().intersectionShader = shaderStageIdx;
        break;
    }
    default: {
    }
    }
}

void RayTracingPipelineBuilder::setSpecializationConstant(
    const uint32_t shaderStageIdx, const uint32_t constantId, const uint32_t value) {
    auto specialization = std::make_unique<StageSpecialization>();
    specialization->entry = {.constantID = constantId, .offset = 0, .size = sizeof(uint32_t)};
    specialization->value = value;
    specialization->info = {
        .mapEntryCount = 1,
        .pMapEntries = &specialization->entry,
        .dataSize = sizeof(uint32_t),
        .pData = &specialization->value,
    };
    m_stages.at(shaderStageIdx).pSpecializationInfo = &specialization->info;
    m_stageSpecializations.push_back(std::move(specialization));
}

void RayTracingPipelineBuilder::setDescriptorHeapMappings(
    const uint32_t shaderStageIdx, const std::span<const VkDescriptorSetAndBindingMappingEXT> mappings) {
    auto stage = std::make_unique<StageMappings>();
    stage->mappings.assign(mappings.begin(), mappings.end());
    stage->info.mappingCount = static_cast<uint32_t>(stage->mappings.size());
    stage->info.pMappings = stage->mappings.data();

    m_stages.at(shaderStageIdx).pNext = &stage->info;
    m_stageMappings.push_back(std::move(stage));
}

VkPipeline RayTracingPipelineBuilder::createHandle(const VkPipelineLayout pipelineLayout) {
    const VkRayTracingPipelineCreateInfoKHR raytracingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(m_stages.size()),
        .pStages = m_stages.data(),
        .groupCount = static_cast<uint32_t>(m_groups.size()),
        .pGroups = m_groups.data(),
        .maxPipelineRayRecursionDepth = 1, // We set up iterative ray tracing in the generation shader.
        .layout = pipelineLayout,
    };
    VkPipeline pipeline{VK_NULL_HANDLE};
    VK_FATAL(vkCreateRayTracingPipelinesKHR(
        m_device.getHandle(), {}, nullptr, 1, &raytracingCreateInfo, nullptr, &pipeline));
    return pipeline;
}

VkPipeline RayTracingPipelineBuilder::createDescriptorHeapHandle() {
    const VkPipelineCreateFlags2CreateInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
        .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
    };
    const VkRayTracingPipelineCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = &flagsInfo,
        .stageCount = static_cast<uint32_t>(m_stages.size()),
        .pStages = m_stages.data(),
        .groupCount = static_cast<uint32_t>(m_groups.size()),
        .pGroups = m_groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = VK_NULL_HANDLE,
    };
    VkPipeline pipeline{VK_NULL_HANDLE};
    VK_FATAL(vkCreateRayTracingPipelinesKHR(m_device.getHandle(), {}, nullptr, 1, &createInfo, nullptr, &pipeline));
    return pipeline;
}

ShaderBindingTable RayTracingPipelineBuilder::createShaderBindingTable(const VkPipeline rayTracingPipeline) {
    const VkDeviceSize baseAlignment =
        m_device.getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupBaseAlignment;
    const auto createShaderHandleBuffer =
        [this, baseAlignment](const VkPipeline rayTracingPipeline, const uint32_t groupCount) {
            const uint32_t handleSize =
                m_device.getPhysicalDevice().getRayTracingPipelineProperties().shaderGroupHandleSize;
            std::vector<uint8_t> shaderHandleBuffer(groupCount * baseAlignment); // NOLINT

            vkGetRayTracingShaderGroupHandlesKHR(
                m_device.getHandle(),
                rayTracingPipeline,
                0,
                groupCount,
                shaderHandleBuffer.size(),
                shaderHandleBuffer.data());

            for (int32_t i = static_cast<int32_t>(groupCount) - 1; i >= 0; --i) {
                memcpy(
                    &shaderHandleBuffer[i * baseAlignment], &shaderHandleBuffer[i * handleSize], handleSize); // NOLINT
            }
            return shaderHandleBuffer;
        };

    std::vector<uint8_t> shaderHandleStorage(
        createShaderHandleBuffer(rayTracingPipeline, static_cast<uint32_t>(m_groups.size())));
    auto buffer = std::make_unique<VulkanBuffer>(
        m_device,
        shaderHandleStorage.size(),
        VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_2_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT,
        BufferMemoryType::GpuOnly);
    m_device.postResourceUpdate(
        [buffer = buffer.get(), handleStorage = std::move(shaderHandleStorage)](const VkCommandBuffer commandBuffer) {
            VulkanCommandEncoder(commandBuffer).updateBuffer(*buffer, std::as_bytes(std::span(handleStorage)));
        });

    std::vector<VkDeviceSize> sizes{0};
    sizes.emplace_back(sizes.back() + m_stageCounts[VK_SHADER_STAGE_RAYGEN_BIT_KHR] * baseAlignment);
    sizes.emplace_back(sizes.back() + m_stageCounts[VK_SHADER_STAGE_MISS_BIT_KHR] * baseAlignment);
    sizes.emplace_back(sizes.back() + m_stageCounts[VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR] * baseAlignment);
    sizes.emplace_back(sizes.back() + m_stageCounts[VK_SHADER_STAGE_CALLABLE_BIT_KHR] * baseAlignment);

    std::array<VkStridedDeviceAddressRegionKHR, 4> bindings{};
    for (int32_t i = 0; i < static_cast<int32_t>(bindings.size()); ++i) {
        bindings[i].deviceAddress = buffer->getDeviceAddress() + sizes[i];
        bindings[i].size = sizes[i + 1] - sizes[i];
        bindings[i].stride = baseAlignment;
    }

    return {
        .buffer = std::move(buffer),
        .bindings = bindings,
    };
}
} // namespace crisp
