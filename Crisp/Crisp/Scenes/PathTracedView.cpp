#include <Crisp/Scenes/PathTracedView.hpp>

#include <algorithm>
#include <ranges>

#include <imgui.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {

constexpr std::array<const char*, 3> kEnergyCompensationNames{"None", "Kulla-Conty", "Turquin"};

constexpr std::array<PathTracerShaderStage, 3> kShaderStages{{
    {"PathTracer/pbr-trace.rgen", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    {"PathTracer/trace.rmiss", VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR},
    {"PathTracer/pbr-trace.rchit", VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR},
}};

struct PathTracedPassData {
    RenderGraphResourceHandle image;
};

} // namespace

PathTracedView::PathTracedView(
    Renderer& renderer, const std::span<const PathTracedGeometry> instances, const VkDeviceAddress materialTableAddress)
    : m_renderer(&renderer) {
    CRISP_CHECK(!instances.empty(), "A path-traced view needs at least one instance.");

    auto& device = m_renderer->getDevice();

    std::vector<PathTracerInstance> tracerInstances;
    tracerInstances.reserve(instances.size());
    for (auto&& [idx, instance] : std::views::enumerate(instances)) {
        tracerInstances.push_back({
            .geometry = instance.geometry,
            .transform = instance.transform,
            .triangleCount = instance.triangleCount,
            .customIndex = static_cast<uint32_t>(idx),
            .sceneIndex = instance.sceneIndex,
            .visibilityMask = instance.visibilityMask,
        });
    }

    const auto materialTextureSlotCount = static_cast<uint32_t>(instances.size()) * kPbrMapTypeCount;
    m_pathTracer = std::make_unique<PathTracer>(
        renderer,
        PathTracerCreateInfo{
            .debugName = "Path-Traced View",
            .shaderStages = kShaderStages,
            .resourceHeapSlotCount = kPathTracerMaterialTextureFirstSlot + materialTextureSlotCount,
            .samplerHeapSlotCount = kPathTracerSamplerHeapSlotCount,
            .integratorParamsSize = sizeof(IntegratorParameters),
        },
        tracerInstances);

    auto& resourceHeap = m_pathTracer->getResourceHeap();
    auto& samplerHeap = m_pathTracer->getSamplerHeap();

    samplerHeap.write(kPathTracerEnvironmentSamplerSlot, createLinearClampSamplerCreateInfo());
    // PBR material textures repeat just like the raster path.
    samplerHeap.write(kPathTracerMaterialSamplerSlot, createLinearRepeatSamplerCreateInfo(MaxAnisotropy));

    m_ggxAlbedoLut = bindGgxAlbedoLut(renderer, *m_pathTracer);

    std::vector<PathTracedInstance> instanceRecords;
    instanceRecords.reserve(instances.size());
    for (auto&& [idx, instance] : std::views::enumerate(instances)) {
        const auto& geometry = *instance.geometry;
        uint32_t materialTextureOffset = kInvalidMaterialTextureOffset;
        if (std::ranges::all_of(instance.materialTextures, [](const VulkanImageView* view) { return view != nullptr; })) {
            materialTextureOffset = kPathTracerMaterialTextureFirstSlot + static_cast<uint32_t>(idx) * kPbrMapTypeCount;
            for (uint32_t textureIndex = 0; textureIndex < kPbrMapTypeCount; ++textureIndex) {
                resourceHeap.writeSampledImage(
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
            .materialIndex = static_cast<int32_t>(instance.materialIndex),
            .materialTextureOffset = materialTextureOffset,
        });
    }

    m_instanceBuffer = createStorageBuffer(
        device, instanceRecords.size() * sizeof(PathTracedInstance), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    device.setObjectName(*m_instanceBuffer, "Path-Traced View Instances");
    fillDeviceBuffer(*m_renderer, m_instanceBuffer.get(), instanceRecords);

    m_sceneAddresses = {
        .instances = m_instanceBuffer->getDeviceAddress(),
        .materials = materialTableAddress,
    };
}

PathTracedView::~PathTracedView() = default;

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
    m_pathTracer->setStorageImage(renderGraph.getImageView<&PathTracedPassData::image>());
    if (m_environmentView != nullptr) {
        m_pathTracer->getResourceHeap().writeSampledImage(
            kPathTracerEnvironmentSlot, *m_environmentView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void PathTracedView::setEnvironmentDistribution(
    const VulkanImageView& equirectView, const std::span<const float> cdf, const uint32_t width, const uint32_t height) {
    CRISP_CHECK(width > 0 && height > 0);
    CRISP_CHECK_EQ(cdf.size(), static_cast<size_t>(height) + 1 + static_cast<size_t>(height) * (width + 1));

    auto& device = m_renderer->getDevice();
    m_environmentView = &equirectView;
    m_pathTracer->getResourceHeap().writeSampledImage(
        kPathTracerEnvironmentSlot, equirectView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_environmentCdfBuffer =
        createStorageBuffer(device, cdf.size() * sizeof(float), VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
    device.setObjectName(*m_environmentCdfBuffer, "Path-Traced View Environment CDF");
    fillDeviceBuffer(*m_renderer, m_environmentCdfBuffer.get(), cdf.data(), cdf.size() * sizeof(float));

    m_sceneAddresses.environmentCdf = m_environmentCdfBuffer->getDeviceAddress();
    m_integratorParams.environmentWidth = static_cast<int32_t>(width);
    m_integratorParams.environmentHeight = static_cast<int32_t>(height);
    resetAccumulation();
}

void PathTracedView::setSceneIndex(const uint32_t sceneIndex) {
    m_pathTracer->setSceneIndex(sceneIndex);
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

    auto& resourceHeap = m_pathTracer->getResourceHeap();
    bool updated = false;
    for (const auto& binding : m_materialTextureBindings) {
        if (binding.materialIndex != materialIndex) {
            continue;
        }
        for (uint32_t textureIndex = 0; textureIndex < kPbrMapTypeCount; ++textureIndex) {
            resourceHeap.writeSampledImage(
                binding.heapOffset + textureIndex, *textures[textureIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        updated = true;
    }
    CRISP_CHECK(updated, "Path-traced material index {} has no texture binding.", materialIndex);
    resetAccumulation();
}

void PathTracedView::updateCamera(const CameraParameters& cameraParams) {
    m_pathTracer->updateCamera(cameraParams);
}

void PathTracedView::resetAccumulation() {
    m_pathTracer->resetAccumulation();
}

int32_t PathTracedView::getAccumulatedSampleCount() const {
    return m_pathTracer->getAccumulatedSampleCount();
}

void PathTracedView::uploadFrameData(const FrameContext& frameContext) {
    m_integratorParams.frameIdx = m_pathTracer->getFrameIndex();
    m_pathTracer->uploadFrameData(frameContext, structAsBytes(m_integratorParams));
}

void PathTracedView::trace(const FrameContext& frameContext) {
    m_pathTracer->trace(frameContext, m_renderer->getSwapChainExtent(), structAsBytes(m_sceneAddresses));
    m_pathTracer->advance(m_integratorParams.sampleCount);
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
