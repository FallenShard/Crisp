#include <Crisp/Models/Atmosphere.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
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

struct TransmittanceLutData {
    RenderGraphResourceHandle lut;
};

struct MultipleScatteringData {
    RenderGraphResourceHandle tex;
};

struct SkyViewLutData {
    RenderGraphResourceHandle lut;
};

struct SkyVolumeLutData {
    RenderGraphResourceHandle lut;
};

constexpr const char* kAtmosphereBufferId = "atmosphereBuffer";
constexpr const char* kLinearClampSamplerId = "linearClamp";
constexpr const char* kVolumeGeometryId = "skyCameraVolumesGeometry";

std::unique_ptr<VulkanPipeline> createMultiScatteringPipeline(Renderer& renderer, const VkExtent3D& workGroupSize) {
    PipelineLayoutBuilder layoutBuilder;
    layoutBuilder
        .defineDescriptorSet(
            0,
            false,
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            })
        .defineDescriptorSet(
            1,
            false,
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            });

    VulkanDevice& device = renderer.getDevice();
    auto layout = layoutBuilder.create(device);

    // The shader declares its work group size through specialization constants 0, 1 and 2.
    const std::vector<VkSpecializationMapEntry> specEntries = {
        {0, 0 * sizeof(uint32_t), sizeof(uint32_t)},
        {1, 1 * sizeof(uint32_t), sizeof(uint32_t)},
        {2, 2 * sizeof(uint32_t), sizeof(uint32_t)},
    };

    VkSpecializationInfo specInfo = {};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = sizeof(workGroupSize);
    specInfo.pData = &workGroupSize;

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = createShaderStageInfo(
        VK_SHADER_STAGE_COMPUTE_BIT, renderer.getOrLoadShaderModule("sky-multiple-scattering.comp"));
    pipelineInfo.stage.pSpecializationInfo = &specInfo;
    pipelineInfo.layout = layout->getHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateComputePipelines(device.getHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    auto result = std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE);
    result->setDebugName(device, MultipleScatteringPass);
    return result;
}

// Wires up the descriptors shared by every atmosphere pass: the parameter buffer in set 0, followed by the LUTs
// this pass samples in set 1, in binding order. The render graph only allocates its physical images during
// compile(), which runs after every pass has been declared, so materials can only be built once a pass first
// executes.
Material* createAtmosphereMaterial(
    Renderer& renderer,
    ResourceContext& resourceContext,
    rg::RenderGraph& renderGraph,
    const std::string& id,
    const std::string& pipelineFilename,
    const std::string& passName,
    const std::vector<RenderGraphResourceHandle>& sampledLuts) {
    VulkanPipeline* pipeline = resourceContext.pipelineCache.loadPipeline(
        id,
        pipelineFilename,
        renderer.getShaderCache(),
        renderer.getDevice(),
        renderGraph.getRasterizationPassDescriptor(passName));
    Material* material = resourceContext.createMaterial(id, pipeline);
    material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kAtmosphereBufferId));

    const VulkanSampler& sampler = resourceContext.imageCache.getSampler(kLinearClampSamplerId);
    for (uint32_t binding = 0; binding < static_cast<uint32_t>(sampledLuts.size()); ++binding) {
        material->writeDescriptor(1, binding, renderGraph.getResourceImageView(sampledLuts[binding]), sampler);
    }
    renderer.getDevice().flushDescriptorUpdates();
    return material;
}
} // namespace

void addAtmosphereRenderPasses(rg::RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext) {
    resourceContext.imageCache.addSampler(kLinearClampSamplerId, createLinearClampSampler(renderer.getDevice()));

    // The camera volume is filled one layer per instance; the geometry shader routes each instance to its slice
    // via gl_Layer, so this pass draws an instanced triangle rather than the shared full screen quad.
    const std::vector<glm::vec2> volumeVertices{{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};
    const std::vector<glm::uvec3> volumeFaces{{0, 2, 1}};
    resourceContext.addGeometry(kVolumeGeometryId, Geometry(renderer, volumeVertices, volumeFaces))
        .setInstanceCount(kCameraVolumeLutSliceCount);

    renderGraph.addPass(
        TransmittanceLutPass,
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
        [&renderer, &resourceContext, &renderGraph, material = static_cast<Material*>(nullptr)](
            const FrameContext& ctx) mutable {
            if (material == nullptr) {
                material = createAtmosphereMaterial(
                    renderer,
                    resourceContext,
                    renderGraph,
                    "transmittanceLut",
                    "SkyTransLut.json",
                    TransmittanceLutPass,
                    {});
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            material->getPipeline()->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);

            // SkyTransLut.json bakes the viewport and scissor from the pass render area, so the pipeline
            // declares neither as dynamic state. Setting them here would violate the static state and also
            // cover the swap chain rather than the LUT.
            renderer.drawFullScreenQuad(cmdBufferHandle);
        });

    renderGraph.addPass(
        MultipleScatteringPass,
        [](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
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
        [&renderer,
         &resourceContext,
         &renderGraph,
         pipeline = std::shared_ptr<VulkanPipeline>{},
         material = std::shared_ptr<Material>{}](const FrameContext& ctx) mutable {
            if (material == nullptr) {
                // Built by hand rather than from a json description because the work group size has to be fed in
                // as a specialization constant. The material is likewise constructed directly instead of through
                // ResourceContext, whose createMaterial() resolves the descriptor allocator through the pipeline
                // cache - this pipeline never goes through the cache, so it has no entry there. The layout built
                // above owns its own allocator, which is what Material picks up from the pipeline.
                pipeline = createMultiScatteringPipeline(renderer, {1, 1, 64});
                material = std::make_shared<Material>(pipeline.get());
                material->setDebugName(MultipleScatteringPass);
                material->writeDescriptor(0, 0, *resourceContext.getRingBuffer(kAtmosphereBufferId));

                auto& blackboard = renderGraph.getBlackboard();
                material->writeDescriptor(
                    1,
                    0,
                    VkDescriptorImageInfo{
                        VK_NULL_HANDLE,
                        renderGraph.getResourceImageView(blackboard.get<MultipleScatteringData>().tex).getHandle(),
                        VK_IMAGE_LAYOUT_GENERAL,
                    });
                material->writeDescriptor(
                    1,
                    1,
                    renderGraph.getResourceImageView(blackboard.get<TransmittanceLutData>().lut),
                    resourceContext.imageCache.getSampler(kLinearClampSamplerId));
                renderer.getDevice().flushDescriptorUpdates();
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            pipeline->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);
            vkCmdDispatch(cmdBufferHandle, kMultiScatteringLutResolution, kMultiScatteringLutResolution, 1);
        });

    renderGraph.addPass(
        SkyViewLutPass,
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
        [&renderer, &resourceContext, &renderGraph, material = static_cast<Material*>(nullptr)](
            const FrameContext& ctx) mutable {
            if (material == nullptr) {
                auto& blackboard = renderGraph.getBlackboard();
                material = createAtmosphereMaterial(
                    renderer,
                    resourceContext,
                    renderGraph,
                    "skyViewLut",
                    "SkyViewLut.json",
                    SkyViewLutPass,
                    {
                        blackboard.get<TransmittanceLutData>().lut,
                        blackboard.get<MultipleScatteringData>().tex,
                    });
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            material->getPipeline()->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);
            renderer.drawFullScreenQuad(cmdBufferHandle);
        });

    renderGraph.addPass(
        ViewVolumePass,
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
        [&renderer, &resourceContext, &renderGraph, material = static_cast<Material*>(nullptr)](
            const FrameContext& ctx) mutable {
            if (material == nullptr) {
                auto& blackboard = renderGraph.getBlackboard();
                material = createAtmosphereMaterial(
                    renderer,
                    resourceContext,
                    renderGraph,
                    "skyCameraVolumes",
                    "SkyCameraVolumes.json",
                    ViewVolumePass,
                    {
                        blackboard.get<TransmittanceLutData>().lut,
                        blackboard.get<MultipleScatteringData>().tex,
                    });
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            material->getPipeline()->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);
            resourceContext.getGeometry(kVolumeGeometryId).bindAndDraw(cmdBufferHandle);
        });

    renderGraph.addPass(
        RayMarchingPass,
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
        [&renderer, &resourceContext, &renderGraph, material = static_cast<Material*>(nullptr)](
            const FrameContext& ctx) mutable {
            if (material == nullptr) {
                auto& blackboard = renderGraph.getBlackboard();
                // Set 1 binding 4 is the view depth texture, which sky-ray-march.frag currently does not sample,
                // so it is intentionally left unwritten until a depth pre-pass feeds it.
                material = createAtmosphereMaterial(
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
            }

            const VkCommandBuffer cmdBufferHandle = ctx.commandEncoder.getHandle();
            material->getPipeline()->bind(cmdBufferHandle);
            material->bind(cmdBufferHandle);
            renderer.drawFullScreenQuad(cmdBufferHandle);
        });
}

} // namespace crisp
