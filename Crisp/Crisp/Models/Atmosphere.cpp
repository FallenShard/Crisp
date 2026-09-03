#include <Crisp/Models/Atmosphere.hpp>

#include <algorithm>
#include <cmath>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
constexpr const char* TransmittanceLutPass = "transmittanceLutPass";
constexpr const char* MultipleScatteringPass = "multipleScatteringPass";
constexpr const char* SkyViewLutPass = "skyViewLutPass";
constexpr const char* ViewVolumePass = "viewVolumePass";
constexpr const char* RayMarchingPass = "rayMarchingPass";

constexpr const char* kAtmosphereBufferId = "atmosphereBuffer";
constexpr const char* kLinearClampSamplerId = "linearClamp";
constexpr const char* kVolumeGeometryId = "skyCameraVolumesGeometry";

std::unique_ptr<VulkanPipeline> createMultiScatteringPipeline(Renderer& renderer, const VkExtent3D& workGroupSize) {
    auto result = createComputePipeline(
        renderer.getDevice(),
        renderer.getAssetPaths().getShaderSpvPath("sky-multiple-scattering.comp"),
        workGroupSize,
        [](PipelineLayoutBuilder& builder) { builder.setDescriptorDynamic(0, 0, true); });
    result->setDebugName(renderer.getDevice(), MultipleScatteringPass);
    return result;
}

// Builds one pass's material: the parameter buffer in set 0, then the LUTs it samples in set 1, in binding
// order. The materials are constructed directly rather than parked in the ResourceContext, since they are
// rebuilt on every compile and nothing else looks them up.
std::unique_ptr<Material> createAtmosphereMaterial(
    Renderer& renderer,
    ResourceContext& resourceContext,
    const rg::RenderGraph& renderGraph,
    const std::string& id,
    const std::string& pipelineFilename,
    const std::string& passName,
    const std::vector<RenderGraphResourceHandle>& sampledLuts) {
    VulkanPipeline* pipeline = resourceContext.pipelineCache.loadPipeline(
        id, pipelineFilename, renderer.getDevice(), {renderGraph.getRasterizationPassDescriptor(passName)});

    // Cached pipelines take their allocator from the cache; only hand-built layouts own one.
    auto material =
        std::make_unique<Material>(pipeline, resourceContext.getDescriptorAllocator(pipeline->getPipelineLayout()));
    material->setDebugName(id);
    material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kAtmosphereBufferId));

    const VulkanSampler& sampler = resourceContext.imageCache.getSampler(kLinearClampSamplerId);
    for (uint32_t binding = 0; binding < static_cast<uint32_t>(sampledLuts.size()); ++binding) {
        material->writeDescriptor(1, binding, renderGraph.getResourceImageView(sampledLuts[binding]), sampler);
    }
    return material;
}
} // namespace

glm::vec3 computeSunDirection(const float azimuthDegrees, const float elevationDegrees) {
    const float azimuth = glm::radians(azimuthDegrees);
    const float elevation = glm::radians(elevationDegrees);
    const float cosElevation = std::cos(elevation);
    return {cosElevation * std::cos(azimuth), std::sin(elevation), cosElevation * std::sin(azimuth)};
}

void applyAtmosphereSettings(const AtmosphereSettings& settings, AtmosphereParameters& params) {
    params.sunDirection = computeSunDirection(settings.sunAzimuthDegrees, settings.sunElevationDegrees);
    params.sunIrradiance = glm::vec4(settings.sunColor * settings.sunIrradianceScale, 1.0f);

    // The shaders evaluate exp(densityScale * altitude), so they want the negated reciprocal scale height.
    params.rayleighDensityScale = -1.0f / std::max(settings.rayleighScaleHeight, 1e-3f);
    params.mieDensityScale = -1.0f / std::max(settings.mieScaleHeight, 1e-3f);

    params.topRadius = params.bottomRadius + settings.atmosphereHeight;

    // Extinction is not independently authored: what is not scattered out of the beam has to be absorbed.
    params.mieExtinction = params.mieScattering + glm::vec3(params.mieAbsorption);
}

void addAtmosphereLutPasses(
    rg::RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext, AtmosphereMaterials& materials) {
    resourceContext.imageCache.addSampler(kLinearClampSamplerId, createLinearClampSampler(renderer.getDevice()));

    // The camera volume is filled one layer per instance; the geometry shader routes each instance to its slice
    // via gl_Layer, so this pass draws an instanced triangle rather than the shared full screen quad.
    const std::vector<glm::vec2> volumeVertices{{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};
    const std::vector<glm::uvec3> volumeFaces{{0, 2, 1}};
    resourceContext.addGeometry(kVolumeGeometryId, Geometry(renderer, volumeVertices, volumeFaces))
        .setInstanceCount(kCameraVolumeLutSliceCount);

    renderGraph.addPass(
        TransmittanceLutPass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<TransmittanceLutData>();
            data.lut = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = kTransmittanceLutWidth,
                    .height = kTransmittanceLutHeight,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                },
                "transmittanceLut");
        },
        [&renderer, &materials](const FrameContext& ctx) {
            const Material& material = *materials.transmittanceLut;
            ctx.commandEncoder.bindPipeline(*material.getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material.getDescriptorSetBinding());

            // SkyTransLut.json bakes the viewport and scissor from the pass render area, so the pipeline
            // declares neither as dynamic state. Setting them here would violate the static state and also
            // cover the swap chain rather than the LUT.
            renderer.drawFullScreenQuad(ctx.commandEncoder);
        });

    renderGraph.addPass(
        MultipleScatteringPass,
        PassType::Compute,
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<TransmittanceLutData>().lut);

            auto& data = builder.getBlackboard().insert<MultipleScatteringData>();
            data.tex = builder.createStorageImage(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = kMultiScatteringLutResolution,
                    .height = kMultiScatteringLutResolution,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                },
                "multiScatTex");
        },
        [&materials](const FrameContext& ctx) {
            ctx.commandEncoder.bindPipeline(*materials.multiScatteringPipeline);
            ctx.commandEncoder.bindDescriptorSets(materials.multiScattering->getDescriptorSetBinding());
            ctx.commandEncoder.dispatchCompute({kMultiScatteringLutResolution, kMultiScatteringLutResolution, 1});
        });

    renderGraph.addPass(
        SkyViewLutPass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<TransmittanceLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<MultipleScatteringData>().tex);

            auto& data = builder.getBlackboard().insert<SkyViewLutData>();
            data.lut = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = kSkyViewLutWidth,
                    .height = kSkyViewLutHeight,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                },
                "skyViewLut");
        },
        [&renderer, &materials](const FrameContext& ctx) {
            const Material& material = *materials.skyViewLut;
            ctx.commandEncoder.bindPipeline(*material.getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material.getDescriptorSetBinding());
            renderer.drawFullScreenQuad(ctx.commandEncoder);
        });

    renderGraph.addPass(
        ViewVolumePass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<TransmittanceLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<MultipleScatteringData>().tex);

            auto& data = builder.getBlackboard().insert<SkyVolumeLutData>();
            data.lut = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = kCameraVolumeLutWidth,
                    .height = kCameraVolumeLutHeight,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .layerCount = kCameraVolumeLutSliceCount,
                },
                "skyVolumeLut");
        },
        [&resourceContext, &materials](const FrameContext& ctx) {
            const Material& material = *materials.cameraVolume;
            ctx.commandEncoder.bindPipeline(*material.getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material.getDescriptorSetBinding());
            resourceContext.getGeometry(kVolumeGeometryId).bindAndDraw(ctx.commandEncoder);
        });
}

void addAtmosphereRenderPasses(
    rg::RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext, AtmosphereMaterials& materials) {
    addAtmosphereLutPasses(renderGraph, renderer, resourceContext, materials);

    renderGraph.addPass(
        RayMarchingPass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<TransmittanceLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<MultipleScatteringData>().tex);
            builder.readTexture(builder.getBlackboard().get<SkyViewLutData>().lut);
            builder.readTexture(builder.getBlackboard().get<SkyVolumeLutData>().lut);

            auto& data = builder.getBlackboard().insert<AtmospherePassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                "rayMarchedImage");
            builder.exportTexture(data.image);
        },
        [&renderer, &materials](const FrameContext& ctx) {
            const Material& material = *materials.rayMarching;
            ctx.commandEncoder.bindPipeline(*material.getPipeline());
            ctx.commandEncoder.bindDescriptorSets(material.getDescriptorSetBinding());
            renderer.drawFullScreenQuad(ctx.commandEncoder);
        });
}

void createAtmosphereLutMaterials(
    AtmosphereMaterials& materials,
    const rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext) {
    const auto& blackboard = renderGraph.getBlackboard();
    const auto transmittanceLut = blackboard.get<TransmittanceLutData>().lut;
    const auto multiScatteringTex = blackboard.get<MultipleScatteringData>().tex;

    materials.transmittanceLut = createAtmosphereMaterial(
        renderer, resourceContext, renderGraph, "transmittanceLut", "SkyTransLut.json", TransmittanceLutPass, {});
    materials.skyViewLut = createAtmosphereMaterial(
        renderer,
        resourceContext,
        renderGraph,
        "skyViewLut",
        "SkyViewLut.json",
        SkyViewLutPass,
        {transmittanceLut, multiScatteringTex});
    materials.cameraVolume = createAtmosphereMaterial(
        renderer,
        resourceContext,
        renderGraph,
        "skyCameraVolumes",
        "SkyCameraVolumes.json",
        ViewVolumePass,
        {transmittanceLut, multiScatteringTex});

    if (materials.multiScatteringPipeline == nullptr) {
        materials.multiScatteringPipeline = createMultiScatteringPipeline(renderer, {1, 1, 64});
    }
    materials.multiScattering = std::make_unique<Material>(materials.multiScatteringPipeline.get());
    materials.multiScattering->setDebugName(MultipleScatteringPass);
    materials.multiScattering->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kAtmosphereBufferId));
    // This pass writes its own LUT, so binding 0 is a storage image rather than a sampled one.
    materials.multiScattering->writeDescriptor(
        1,
        0,
        VkDescriptorImageInfo{
            VK_NULL_HANDLE,
            renderGraph.getResourceImageView(multiScatteringTex).getHandle(),
            VK_IMAGE_LAYOUT_GENERAL,
        });
    materials.multiScattering->writeDescriptor(
        1,
        1,
        renderGraph.getResourceImageView(transmittanceLut),
        resourceContext.imageCache.getSampler(kLinearClampSamplerId));

    renderer.getDevice().flushDescriptorUpdates();
}

void createAtmosphereRenderMaterials(
    AtmosphereMaterials& materials,
    const rg::RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext) {
    createAtmosphereLutMaterials(materials, renderGraph, renderer, resourceContext);

    const auto& blackboard = renderGraph.getBlackboard();
    // Set 1 binding 4 is the view depth texture, which sky-ray-march.frag currently does not sample, so it is
    // intentionally left unwritten until a depth pre-pass feeds it.
    materials.rayMarching = createAtmosphereMaterial(
        renderer,
        resourceContext,
        renderGraph,
        "rayMarching",
        "SkyRayMarching.json",
        RayMarchingPass,
        {
            blackboard.get<TransmittanceLutData>().lut,
            blackboard.get<MultipleScatteringData>().tex,
            blackboard.get<SkyViewLutData>().lut,
            blackboard.get<SkyVolumeLutData>().lut,
        });

    renderer.getDevice().flushDescriptorUpdates();
}

} // namespace crisp
