#include <Crisp/Scenes/PathTracer.hpp>

#include <algorithm>
#include <cstring>
#include <ranges>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/GgxAlbedoLut.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {

PathTracer::PathTracer(
    Renderer& renderer, const PathTracerCreateInfo& createInfo, const std::span<const PathTracerInstance> instances)
    : m_renderer(&renderer)
    , m_integratorParamsSize(createInfo.integratorParamsSize) {
    CRISP_CHECK(!instances.empty(), "A path tracer needs at least one instance.");
    CRISP_CHECK_GE(createInfo.resourceHeapSlotCount, kPathTracerFirstFreeSlot);

    auto& device = m_renderer->getDevice();
    m_resourceHeap = std::make_unique<VulkanResourceHeap>(
        device, createInfo.resourceHeapSlotCount, fmt::format("{} Resource Heap", createInfo.debugName));
    m_samplerHeap = std::make_unique<VulkanSamplerHeap>(
        device, createInfo.samplerHeapSlotCount, fmt::format("{} Sampler Heap", createInfo.debugName));

    createAccelerationStructures(instances, createInfo.debugName);

    m_cameraBuffer = std::make_unique<VulkanBuffer>(
        device,
        sizeof(CameraParameters),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::GpuOnly);
    device.setObjectName(*m_cameraBuffer, fmt::format("{} Camera", createInfo.debugName));
    m_resourceHeap->writeUniformBuffer(kPathTracerViewSlot, *m_cameraBuffer);

    if (m_integratorParamsSize > 0) {
        m_integratorBuffer = std::make_unique<VulkanBuffer>(
            device,
            m_integratorParamsSize,
            VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
            BufferMemoryType::GpuOnly);
        device.setObjectName(*m_integratorBuffer, fmt::format("{} Integrator", createInfo.debugName));
        m_resourceHeap->writeUniformBuffer(kPathTracerIntegratorSlot, *m_integratorBuffer);
    }

    createPipeline(createInfo);
}

PathTracer::~PathTracer() = default;

void PathTracer::createAccelerationStructures(
    const std::span<const PathTracerInstance> instances, const std::string& debugName) {
    auto& device = m_renderer->getDevice();

    uint32_t sceneCount = 0;
    for (const auto& instance : instances) {
        sceneCount = std::max(sceneCount, instance.sceneIndex + 1);
    }
    std::vector<std::vector<VulkanAccelerationStructure*>> sceneBlases(sceneCount);
    std::vector<std::vector<uint32_t>> sceneInstances(sceneCount);

    m_bottomLevelAccelStructures.reserve(instances.size());
    for (auto&& [idx, instance] : std::views::enumerate(instances)) {
        CRISP_CHECK(instance.geometry != nullptr, "Path tracer instance {} has no geometry.", idx);
        m_bottomLevelAccelStructures.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                device,
                createAccelerationStructureGeometry(*instance.geometry, 0),
                instance.triangleCount,
                instance.transform));
        m_bottomLevelAccelStructures.back()->setDebugName(device, fmt::format("{} BLAS [{}]", debugName, idx));
        sceneBlases[instance.sceneIndex].push_back(m_bottomLevelAccelStructures.back().get());
        sceneInstances[instance.sceneIndex].push_back(static_cast<uint32_t>(idx));
    }

    m_topLevelAccelStructures.reserve(sceneCount);
    for (uint32_t sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex) {
        CRISP_CHECK(!sceneBlases[sceneIndex].empty(), "Path tracer scene groups must be contiguous and non-empty.");
        auto tlas = std::make_unique<VulkanAccelerationStructure>(device, sceneBlases[sceneIndex]);
        for (auto&& [localIndex, globalIndex] : std::views::enumerate(sceneInstances[sceneIndex])) {
            tlas->setInstanceCustomIndex(static_cast<uint32_t>(localIndex), instances[globalIndex].customIndex);
            tlas->setInstanceMask(static_cast<uint32_t>(localIndex), instances[globalIndex].visibilityMask);
        }
        tlas->setDebugName(device, fmt::format("{} TLAS [{}]", debugName, sceneIndex));
        m_topLevelAccelStructures.push_back(std::move(tlas));
    }

    m_resourceHeap->writeAccelerationStructure(kPathTracerBvhSlot, *m_topLevelAccelStructures[m_sceneIndex]);

    m_renderer->enqueueResourceUpdate([this](const VulkanCommandEncoder& encoder) {
        for (auto& blas : m_bottomLevelAccelStructures) {
            encoder.buildAccelerationStructure(*blas);
        }
        encoder.insertBarrier(kAccelerationStructureWrite >> kAccelerationStructureRead);
        for (auto& tlas : m_topLevelAccelStructures) {
            encoder.buildAccelerationStructure(*tlas);
        }
    });
}

void PathTracer::createPipeline(const PathTracerCreateInfo& createInfo) {
    CRISP_CHECK(!createInfo.shaderStages.empty(), "A path tracer needs at least a ray generation shader.");

    for (uint32_t variant = 0; variant < m_pipelines.size(); ++variant) {
        RayTracingPipelineBuilder pipelineBuilder(m_renderer->getDevice());
        for (auto&& [idx, stage] : std::views::enumerate(createInfo.shaderStages)) {
            pipelineBuilder.addShaderStage(m_renderer->getAssetPaths().getShaderSpvPath(std::string{stage.name}));
            pipelineBuilder.addShaderGroup(static_cast<uint32_t>(idx), stage.groupType);
        }
        // The raygen's sampler reserves three extra dimensions only in the volume variant.
        pipelineBuilder.setSpecializationConstant(0, 0, variant);

        const VkDescriptorSetAndBindingMappingEXT bvhMapping{
            m_resourceHeap->makeMapping(kPathTracerBvhSlot, 1, 0, VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT)};
        pipelineBuilder.setDescriptorHeapMappings(0, {&bvhMapping, 1});

        const VkPipeline pipeline{pipelineBuilder.createDescriptorHeapHandle()};
        m_shaderBindingTables[variant] = pipelineBuilder.createShaderBindingTable(pipeline);
        const char* variantName = variant == 0 ? "Surface" : "Volume";
        m_renderer->getDevice().setObjectName(
            *m_shaderBindingTables[variant].buffer,
            fmt::format("{} {} Shader Binding Table", createInfo.debugName, variantName));

        m_pipelines[variant] = std::make_unique<VulkanPipeline>(
            m_renderer->getDevice(), pipeline, nullptr, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
        m_pipelines[variant]->setDebugName(
            m_renderer->getDevice(), fmt::format("{} {}", createInfo.debugName, variantName));
    }
}

void PathTracer::setStorageImage(const VulkanImageView& imageView) {
    m_resourceHeap->writeStorageImage(kPathTracerImageSlot, imageView, VK_IMAGE_LAYOUT_GENERAL);
}

void PathTracer::setSceneIndex(const uint32_t sceneIndex) {
    CRISP_CHECK_LT(sceneIndex, m_topLevelAccelStructures.size());
    m_sceneIndex = sceneIndex;
    m_resourceHeap->writeAccelerationStructure(kPathTracerBvhSlot, *m_topLevelAccelStructures[m_sceneIndex]);
    resetAccumulation();
}

void PathTracer::setHasParticipatingMedia(const bool hasParticipatingMedia) {
    if (m_hasParticipatingMedia != hasParticipatingMedia) {
        m_hasParticipatingMedia = hasParticipatingMedia;
        resetAccumulation();
    }
}

void PathTracer::updateCamera(const CameraParameters& cameraParams) {
    if (std::memcmp(&cameraParams, &m_cameraParams, sizeof(CameraParameters)) != 0) { // NOLINT
        m_cameraParams = cameraParams;
        resetAccumulation();
    }
}

void PathTracer::resetAccumulation() {
    m_accumulatedSampleCount = 0;
}

void PathTracer::advance(const int32_t sampleCount) {
    m_accumulatedSampleCount += sampleCount;
}

void PathTracer::uploadFrameData(const FrameContext& frameContext, const std::span<const std::byte> integratorParams) {
    CRISP_CHECK_EQ(
        integratorParams.size(),
        m_integratorParamsSize,
        "Integrator parameters do not match the size the path tracer's buffer was created with.");

    frameContext.commandEncoder.insertBarrier(kRayTracingRead >> kTransferWrite);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_cameraBuffer, 0, m_cameraParams);
    if (!integratorParams.empty()) {
        frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_integratorBuffer, 0, integratorParams);
    }
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kRayTracingRead);
}

void PathTracer::trace(
    const FrameContext& frameContext, const VkExtent2D extent, const std::span<const std::byte> pushData) {
    CRISP_CHECK_LE(
        pushData.size(),
        m_renderer->getPhysicalDevice().getDescriptorHeapProperties().maxPushDataSize,
        "Path tracer push data exceeds the descriptor-heap limit.");

    const auto& encoder = frameContext.commandEncoder;
    uploadIfPending(*m_resourceHeap, encoder, *frameContext.stagingBelt, kRayTracingResourceHeapRead);
    uploadIfPending(*m_samplerHeap, encoder, *frameContext.stagingBelt, kRayTracingSamplerHeapRead);

    const uint32_t variant = m_hasParticipatingMedia ? 1u : 0u;
    encoder.bindPipeline(*m_pipelines[variant]);
    encoder.bindResourceHeap(*m_resourceHeap);
    encoder.bindSamplerHeap(*m_samplerHeap);
    encoder.pushData(pushData);
    encoder.traceRays(m_shaderBindingTables[variant].bindings, extent);
}

std::unique_ptr<VulkanImage> bindGgxAlbedoLut(Renderer& renderer, PathTracer& pathTracer) {
    // The table is endpoint-mapped, so repeating would wrap the grazing corner onto the normal-incidence one.
    pathTracer.getSamplerHeap().write(kPathTracerGgxAlbedoLutSamplerSlot, createLinearClampSamplerCreateInfo());

    auto lut = loadGgxAlbedoLut(renderer.getDevice(), renderer.getResourcesPath() / "Textures/GgxAlbedoLut.exr");
    pathTracer.getResourceHeap().writeSampledImage(
        kPathTracerGgxAlbedoLutSlot, lut->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return lut;
}

} // namespace crisp
