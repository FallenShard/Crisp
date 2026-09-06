#include <Crisp/Scenes/PathTracedView.hpp>

#include <algorithm>
#include <cstring>
#include <ranges>

#include <imgui.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/GgxAlbedoLut.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {

template <typename T>
std::span<const std::byte> structAsBytes(const T& value) {
    return std::span<const std::byte>{reinterpret_cast<const std::byte*>(&value), sizeof(value)}; // NOLINT
}

// Must match the heap subscripts in Shaders/pbr-path-trace.rgen.glsl, .rmiss.glsl and .rchit.glsl. The BVH slot
// is reached through a (set, binding) mapping instead, so its number is private here.
constexpr uint32_t kBvhSlot = 0;
constexpr uint32_t kImageSlot = 1;
constexpr uint32_t kViewSlot = 2;
constexpr uint32_t kIntegratorSlot = 3;
constexpr uint32_t kEnvironmentMapSlot = 4;
constexpr uint32_t kGgxAlbedoLutSlot = 5;
constexpr uint32_t kMaterialTextureFirstSlot = 6;

constexpr uint32_t kEnvironmentSamplerSlot = 0;
constexpr uint32_t kMaterialSamplerSlot = 1;
constexpr uint32_t kGgxAlbedoLutSamplerSlot = 2;
constexpr uint32_t kSamplerHeapSlotCount = 3;

constexpr std::array<const char*, 3> kEnergyCompensationNames{"None", "Kulla-Conty", "Turquin"};

struct PathTracedPassData {
    RenderGraphResourceHandle image;
};

} // namespace

PathTracedView::PathTracedView(
    Renderer& renderer,
    const std::span<const PathTracedGeometry> instances,
    const VkDeviceAddress materialTableAddress,
    const VulkanImageView& environmentMapView)
    : m_renderer(&renderer)
    , m_environmentMapView(&environmentMapView) {
    CRISP_CHECK(!instances.empty(), "A path-traced view needs at least one instance.");

    auto& device = m_renderer->getDevice();
    const auto materialTextureSlotCount = static_cast<uint32_t>(instances.size()) * kPbrMapTypeCount;
    m_resourceHeap = std::make_unique<VulkanResourceHeap>(
        device, kMaterialTextureFirstSlot + materialTextureSlotCount, "Path-Traced View Resource Heap");
    m_samplerHeap = std::make_unique<VulkanSamplerHeap>(device, kSamplerHeapSlotCount, "Path-Traced View Sampler Heap");

    // The environment clamps at cube edges; PBR material textures repeat just like the raster path.
    m_samplerHeap->write(kEnvironmentSamplerSlot, createLinearClampSamplerCreateInfo());
    m_samplerHeap->write(kMaterialSamplerSlot, createLinearRepeatSamplerCreateInfo(MaxAnisotropy));

    // The table is endpoint-mapped, so repeating would wrap the grazing corner onto the normal-incidence one.
    m_samplerHeap->write(kGgxAlbedoLutSamplerSlot, createLinearClampSamplerCreateInfo());

    m_ggxAlbedoLut = loadGgxAlbedoLut(device, m_renderer->getResourcesPath() / "Textures/GgxAlbedoLut.exr");
    m_resourceHeap->writeSampledImage(
        kGgxAlbedoLutSlot, m_ggxAlbedoLut->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    std::vector<PathTracedInstance> instanceRecords;
    instanceRecords.reserve(instances.size());
    uint32_t sceneCount = 0;
    for (const auto& instance : instances) {
        sceneCount = std::max(sceneCount, instance.sceneIndex + 1);
    }
    std::vector<std::vector<VulkanAccelerationStructure*>> sceneBlases(sceneCount);
    std::vector<std::vector<uint32_t>> sceneInstanceIndices(sceneCount);

    for (auto&& [idx, instance] : std::views::enumerate(instances)) {
        const auto& geometry = *instance.geometry;
        uint32_t materialTextureOffset = std::numeric_limits<uint32_t>::max();
        if (std::ranges::all_of(instance.materialTextures, [](const VulkanImageView* view) { return view != nullptr; })) {
            materialTextureOffset = kMaterialTextureFirstSlot + static_cast<uint32_t>(idx) * kPbrMapTypeCount;
            for (uint32_t textureIndex = 0; textureIndex < kPbrMapTypeCount; ++textureIndex) {
                m_resourceHeap->writeSampledImage(
                    materialTextureOffset + textureIndex,
                    *instance.materialTextures[textureIndex],
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            m_materialTextureBindings.push_back({
                .materialIndex = instance.materialIndex,
                .heapOffset = materialTextureOffset,
            });
        }
        instanceRecords.push_back({
            .positions = geometry.getVertexBuffer(0)->getDeviceAddress(),
            .attributes = geometry.getVertexBuffer(1)->getDeviceAddress(),
            .triangles = geometry.getIndexBuffer()->getDeviceAddress(),
            .materialIndex = instance.materialIndex,
            .materialTextureOffset = materialTextureOffset,
        });

        m_bottomLevelAccelStructures.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                device, createAccelerationStructureGeometry(geometry, 0), instance.triangleCount, instance.transform));
        m_bottomLevelAccelStructures.back()->setDebugName(device, fmt::format("Path-Traced View BLAS [{}]", idx));
        sceneBlases[instance.sceneIndex].push_back(m_bottomLevelAccelStructures.back().get());
        sceneInstanceIndices[instance.sceneIndex].push_back(static_cast<uint32_t>(idx));
    }

    m_topLevelAccelStructures.reserve(sceneCount);
    for (uint32_t sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex) {
        CRISP_CHECK(!sceneBlases[sceneIndex].empty(), "Path-traced scene groups must be contiguous and non-empty.");
        auto tlas = std::make_unique<VulkanAccelerationStructure>(device, sceneBlases[sceneIndex]);
        for (auto&& [localIndex, globalIndex] : std::views::enumerate(sceneInstanceIndices[sceneIndex])) {
            tlas->setInstanceCustomIndex(static_cast<uint32_t>(localIndex), globalIndex);
            tlas->setInstanceMask(static_cast<uint32_t>(localIndex), instances[globalIndex].visibilityMask);
        }
        tlas->setDebugName(device, fmt::format("Path-Traced View TLAS [{}]", sceneIndex));
        m_topLevelAccelStructures.push_back(std::move(tlas));
    }

    m_instanceBuffer = createStorageBuffer(
        device, instanceRecords.size() * sizeof(PathTracedInstance), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    device.setObjectName(*m_instanceBuffer, "Path-Traced View Instances");
    fillDeviceBuffer(*m_renderer, m_instanceBuffer.get(), instanceRecords);

    m_cameraBuffer = std::make_unique<VulkanBuffer>(
        device,
        sizeof(CameraParameters),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::GpuOnly);
    m_integratorBuffer = std::make_unique<VulkanBuffer>(
        device,
        sizeof(IntegratorParameters),
        VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
        BufferMemoryType::GpuOnly);

    m_sceneAddresses = {
        .instances = m_instanceBuffer->getDeviceAddress(),
        .materials = materialTableAddress,
    };
    CRISP_CHECK_LE(
        sizeof(m_sceneAddresses),
        m_renderer->getPhysicalDevice().getDescriptorHeapProperties().maxPushDataSize,
        "Path-traced view scene addresses exceed the descriptor-heap push-data limit.");

    m_renderer->enqueueResourceUpdate([this](const VulkanCommandEncoder& encoder) {
        for (auto& blas : m_bottomLevelAccelStructures) {
            encoder.buildAccelerationStructure(*blas);
        }
        encoder.insertBarrier(kAccelerationStructureWrite >> kAccelerationStructureRead);
        for (auto& tlas : m_topLevelAccelStructures) {
            encoder.buildAccelerationStructure(*tlas);
        }
    });

    m_pipeline = createPipeline();
}

PathTracedView::~PathTracedView() = default;

std::unique_ptr<VulkanPipeline> PathTracedView::createPipeline() {
    const std::array<std::pair<std::string, VkRayTracingShaderGroupTypeKHR>, 3> shaderInfos{{
        {"pbr-path-trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"pbr-path-trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
        {"pbr-path-trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
    }};

    RayTracingPipelineBuilder pipelineBuilder(m_renderer->getDevice());
    for (auto&& [idx, info] : std::views::enumerate(shaderInfos)) {
        pipelineBuilder.addShaderStage(m_renderer->getAssetPaths().getShaderSpvPath(info.first));
        pipelineBuilder.addShaderGroup(static_cast<uint32_t>(idx), info.second);
    }

    const VkDescriptorSetAndBindingMappingEXT bvhMapping{
        m_resourceHeap->makeMapping(kBvhSlot, 1, 0, VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT)};
    pipelineBuilder.setDescriptorHeapMappings(0, {&bvhMapping, 1});

    const VkPipeline pipeline{pipelineBuilder.createDescriptorHeapHandle()};
    m_shaderBindingTable = pipelineBuilder.createShaderBindingTable(pipeline);
    m_renderer->getDevice().setObjectName(*m_shaderBindingTable.buffer, "Path-Traced View Shader Binding Table");

    auto result = std::make_unique<VulkanPipeline>(
        m_renderer->getDevice(), pipeline, nullptr, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
    result->setDebugName(m_renderer->getDevice(), "Path-Traced View");
    return result;
}

void addPathTracedViewPass(rg::RenderGraph& renderGraph, std::function<void(const FrameContext&)> execute) {
    renderGraph.addPass(
        "path-traced-view",
        PassType::RayTracing,
        [](rg::RenderGraph::Builder& builder) {
            builder.getBlackboard().insert<PathTracedPassData>().image = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                },
                "path-traced-view-accumulation");
        },
        std::move(execute));
    renderGraph.exportTexture(renderGraph.getBlackboard().get<PathTracedPassData>().image);
}

const VulkanImageView& getPathTracedViewImage(const rg::RenderGraph& renderGraph) {
    return renderGraph.getImageView<&PathTracedPassData::image>();
}

void PathTracedView::updateDescriptorHeap(const rg::RenderGraph& renderGraph) {
    m_resourceHeap->writeAccelerationStructure(kBvhSlot, *m_topLevelAccelStructures[m_sceneIndex]);
    m_resourceHeap->writeStorageImage(
        kImageSlot, renderGraph.getImageView<&PathTracedPassData::image>(), VK_IMAGE_LAYOUT_GENERAL);
    m_resourceHeap->writeUniformBuffer(kViewSlot, *m_cameraBuffer);
    m_resourceHeap->writeUniformBuffer(kIntegratorSlot, *m_integratorBuffer);
    m_resourceHeap->writeSampledImage(
        kEnvironmentMapSlot, *m_environmentMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void PathTracedView::setEnvironmentMap(const VulkanImageView& environmentMapView) {
    m_environmentMapView = &environmentMapView;
    m_resourceHeap->writeSampledImage(
        kEnvironmentMapSlot, *m_environmentMapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    resetAccumulation();
}

void PathTracedView::setSceneIndex(const uint32_t sceneIndex) {
    CRISP_CHECK_LT(sceneIndex, m_topLevelAccelStructures.size());
    m_sceneIndex = sceneIndex;
    m_resourceHeap->writeAccelerationStructure(kBvhSlot, *m_topLevelAccelStructures[m_sceneIndex]);
    resetAccumulation();
}

void PathTracedView::setEnvironmentIntensity(const float intensity) {
    m_integratorParams.environmentIntensity = intensity;
    resetAccumulation();
}

void PathTracedView::setVisibilityMask(const uint8_t mask) {
    if (m_integratorParams.visibilityMask != mask) {
        m_integratorParams.visibilityMask = mask;
        resetAccumulation();
    }
}

void PathTracedView::setEnergyCompensation(const EnergyCompensation mode) {
    m_integratorParams.energyCompensation = static_cast<uint32_t>(mode);
    resetAccumulation();
}

void PathTracedView::setMaterialTextures(
    const uint32_t materialIndex, const std::array<const VulkanImageView*, kPbrMapTypeCount>& textures) {
    CRISP_CHECK(
        std::ranges::all_of(textures, [](const VulkanImageView* view) { return view != nullptr; }),
        "Path-traced material textures must all resolve, including fallbacks.");

    bool updated = false;
    for (const auto& binding : m_materialTextureBindings) {
        if (binding.materialIndex != materialIndex) {
            continue;
        }
        for (uint32_t textureIndex = 0; textureIndex < kPbrMapTypeCount; ++textureIndex) {
            m_resourceHeap->writeSampledImage(
                binding.heapOffset + textureIndex, *textures[textureIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        updated = true;
    }
    CRISP_CHECK(updated, "Path-traced material index {} has no texture binding.", materialIndex);
    resetAccumulation();
}

void PathTracedView::updateCamera(const CameraParameters& cameraParams) {
    // Only an actual change may reset accumulation; comparing the parameters keeps a still camera converging.
    if (std::memcmp(&cameraParams, &m_cameraParams, sizeof(CameraParameters)) != 0) {
        m_cameraParams = cameraParams;
        resetAccumulation();
    }
}

void PathTracedView::resetAccumulation() {
    m_integratorParams.frameIdx = 0;
}

int32_t PathTracedView::getAccumulatedSampleCount() const {
    return m_integratorParams.frameIdx * m_integratorParams.sampleCount;
}

void PathTracedView::uploadFrameData(const FrameContext& frameContext) {
    frameContext.commandEncoder.insertBarrier(kRayTracingRead >> kTransferWrite);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_cameraBuffer, 0, m_cameraParams);
    frameContext.stagingBelt->uploadBuffer(frameContext.commandEncoder, *m_integratorBuffer, 0, m_integratorParams);
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kRayTracingRead);
}

void PathTracedView::trace(const FrameContext& frameContext) {
    const auto& encoder = frameContext.commandEncoder;
    uploadIfPending(*m_resourceHeap, encoder, *frameContext.stagingBelt, kRayTracingResourceHeapRead);
    uploadIfPending(*m_samplerHeap, encoder, *frameContext.stagingBelt, kRayTracingSamplerHeapRead);

    encoder.bindPipeline(*m_pipeline);
    encoder.bindResourceHeap(*m_resourceHeap);
    encoder.bindSamplerHeap(*m_samplerHeap);
    encoder.pushData(structAsBytes(m_sceneAddresses));
    encoder.traceRays(m_shaderBindingTable.bindings, m_renderer->getSwapChainExtent());

    ++m_integratorParams.frameIdx;
}

void PathTracedView::drawGui(const bool allowEnvironmentIntensity) {
    ImGui::LabelText("Acc. Samples", "%d", getAccumulatedSampleCount()); // NOLINT
    if (ImGui::SliderInt("Max Bounces", &m_integratorParams.maxBounces, 1, 32)) {
        resetAccumulation();
    }
    if (ImGui::SliderInt("Samples per Frame", &m_integratorParams.sampleCount, 1, 16)) {
        resetAccumulation();
    }
    if (allowEnvironmentIntensity) {
        if (ImGui::SliderFloat("Environment Intensity", &m_integratorParams.environmentIntensity, 0.0f, 4.0f, "%.2f")) {
            resetAccumulation();
        }
    } else {
        ImGui::LabelText("Environment Intensity", "1.00 (unit radiance)");
    }

    int compensation = static_cast<int>(m_integratorParams.energyCompensation);
    if (ImGui::Combo(
            "Multiscatter Compensation",
            &compensation,
            kEnergyCompensationNames.data(),
            static_cast<int>(kEnergyCompensationNames.size()))) {
        setEnergyCompensation(static_cast<EnergyCompensation>(compensation));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip(
            "Energy the single-scattering GGX lobe drops between microfacets. Compare under the white furnace: "
            "uncompensated rough metal darkens, and Turquin gains its brightness at the cost of reciprocity.");
    }
}

} // namespace crisp
