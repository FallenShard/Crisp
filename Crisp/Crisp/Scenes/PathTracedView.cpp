#include <Crisp/Scenes/PathTracedView.hpp>

#include <algorithm>
#include <cstring>
#include <ranges>

#include <imgui.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {

template <typename T>
std::span<const std::byte> structAsBytes(const T& value) {
    return std::span<const std::byte>{reinterpret_cast<const std::byte*>(&value), sizeof(value)}; // NOLINT
}

// Must match the heap array subscripts in Shaders/pbr-path-trace.rgen.glsl and .rmiss.glsl. The BVH slot is
// reached through a (set, binding) mapping rather than a subscript, so its number is private to this file.
constexpr uint32_t kBvhSlot = 0;
constexpr uint32_t kImageSlot = 1;
constexpr uint32_t kViewSlot = 2;
constexpr uint32_t kIntegratorSlot = 3;
constexpr uint32_t kEnvironmentMapSlot = 4;
constexpr uint32_t kHeapSlotCount = 5;

constexpr uint32_t kEnvironmentSamplerSlot = 0;
constexpr uint32_t kSamplerHeapSlotCount = 1;

} // namespace

struct PathTracedPassData {
    RenderGraphResourceHandle image;
};

PathTracedView::PathTracedView(
    Renderer& renderer,
    const std::span<const PathTracedGeometry> instances,
    const VkDeviceAddress materialTableAddress,
    const VulkanImageView& environmentMapView)
    : m_renderer(&renderer)
    , m_environmentMapView(&environmentMapView) {
    CRISP_CHECK(!instances.empty(), "A path-traced view needs at least one instance.");

    auto& device = m_renderer->getDevice();

    std::vector<PathTracedInstance> instanceRecords;
    instanceRecords.reserve(instances.size());
    std::vector<VulkanAccelerationStructure*> blases;
    blases.reserve(instances.size());

    for (auto&& [idx, instance] : std::views::enumerate(instances)) {
        const auto& geometry = *instance.geometry;
        instanceRecords.push_back({
            .positions = geometry.getVertexBuffer(0)->getDeviceAddress(),
            .attributes = geometry.getVertexBuffer(1)->getDeviceAddress(),
            .triangles = geometry.getIndexBuffer()->getDeviceAddress(),
            .materialIndex = instance.materialIndex,
        });

        m_bottomLevelAccelStructures.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                device,
                createAccelerationStructureGeometry(geometry, 0),
                instance.triangleCount,
                instance.transform));
        m_bottomLevelAccelStructures.back()->setDebugName(device, fmt::format("Path-Traced View BLAS [{}]", idx));
        blases.push_back(m_bottomLevelAccelStructures.back().get());
    }

    m_topLevelAccelStructure = std::make_unique<VulkanAccelerationStructure>(device, blases);
    m_topLevelAccelStructure->setDebugName(device, "Path-Traced View TLAS");

    m_instanceBuffer = createStorageBuffer(
        device,
        instanceRecords.size() * sizeof(PathTracedInstance),
        VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
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
        encoder.buildAccelerationStructure(*m_topLevelAccelStructure);
    });

    m_resourceHeap =
        std::make_unique<VulkanResourceHeap>(device, kHeapSlotCount, "Path-Traced View Resource Heap");
    m_samplerHeap = std::make_unique<VulkanSamplerHeap>(device, kSamplerHeapSlotCount, "Path-Traced View Sampler Heap");

    // Clamped trilinear, matching how the raster path samples the same cube map.
    m_samplerHeap->write(kEnvironmentSamplerSlot, createLinearClampSamplerCreateInfo());

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
    m_resourceHeap->writeAccelerationStructure(kBvhSlot, *m_topLevelAccelStructure);
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

void PathTracedView::drawGui() {
    ImGui::LabelText("Acc. Samples", "%d", getAccumulatedSampleCount()); // NOLINT
    if (ImGui::SliderInt("Max Bounces", &m_integratorParams.maxBounces, 1, 32)) {
        resetAccumulation();
    }
    if (ImGui::SliderInt("Samples per Frame", &m_integratorParams.sampleCount, 1, 16)) {
        resetAccumulation();
    }
    if (ImGui::SliderFloat("Environment Intensity", &m_integratorParams.environmentIntensity, 0.0f, 4.0f, "%.2f")) {
        resetAccumulation();
    }
}

} // namespace crisp
