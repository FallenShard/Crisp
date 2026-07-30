#include <Crisp/Models/Atmosphere.hpp>

#include <Crisp/Geometry/Geometry.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/Renderer.hpp>
#include <Crisp/Renderer/ResourceContext.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
constexpr const char* TransmittanceLutPass = "transmittanceLutPass";
constexpr const char* MultipleScatteringPass = "multipleScatteringPass";
constexpr const char* SkyViewLutPass = "skyViewLutPass";
constexpr const char* ViewVolumePass = "viewVolumePass";
constexpr const char* RayMarchingPass = "rayMarchingPass";

AtmosphereParameters::AtmosphereParameters() {
    const glm::mat4 testP = /*glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) **/
        glm::perspective(glm::radians(66.6f), 1280.0f / 720.0f, 0.1f, 20000.0f);
    const glm::mat4 testV =
        glm::lookAt(glm::vec3(0.0f, 0.5f, 1.0f), glm::vec3(0.0f, 0.5f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    VP = testP * testV;
    invVP = glm::inverse(VP);
}

#if 0
std::unique_ptr<VulkanRenderPass> createTransmittanceLutPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        TransmittanceLut,
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setLayerAndMipLevelCount(1)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize({256, 64}, false)
            .create(device));

    return VulkanRenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, {256, 64}, renderTargets);
}

std::unique_ptr<VulkanRenderPass> createSkyViewLutPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        SkyViewLut,
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setLayerAndMipLevelCount(1)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize({192, 108}, false)
            .create(device));

    return VulkanRenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, {192, 108}, renderTargets);
}

std::unique_ptr<VulkanRenderPass> createSkyVolumePass(const VulkanDevice& device, RenderTargetCache& renderTargetCache) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "SkyVolumeLut",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .setDepthSliceCount(32 * kRendererVirtualFrameCount)
            .setCreateFlags(VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize({32, 32}, false)
            .create(device));

    return VulkanRenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0, 0, 32)
        .setAttachmentBufferOverDepthSlices(0, true)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, {32, 32}, renderTargets);
}

std::unique_ptr<VulkanRenderPass> createRayMarchingPass(
    const VulkanDevice& device, RenderTargetCache& renderTargetCache, VkExtent2D renderArea) {
    std::vector<RenderTarget*> renderTargets(1);
    renderTargets[0] = renderTargetCache.addRenderTarget(
        "RayMarchedAtmosphere",
        RenderTargetBuilder()
            .setFormat(VK_FORMAT_R32G32B32A32_SFLOAT)
            .configureColorRenderTarget(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .setSize(renderArea, true)
            .create(device));

    return VulkanRenderPassBuilder()
        .setAttachmentCount(1)
        .setAttachmentMapping(0, 0)
        .setAttachmentOps(0, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
        .setAttachmentLayouts(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)

        .setNumSubpasses(1)
        .addColorAttachmentRef(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .addDependency(
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
        .create(device, renderArea, renderTargets);
}

std::unique_ptr<VulkanPipeline> createMultiScatPipeline(Renderer& renderer, const VkExtent3D& workGroupSize) {
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
            true,
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            });

    VulkanDevice& device = renderer.getDevice();
    auto layout = layoutBuilder.create(device);

    std::vector<VkSpecializationMapEntry> specEntries = {
        //   id,               offset,             size
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

    return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE);
}

FlatHashMap<std::string, std::unique_ptr<RenderNode>> addAtmosphereRenderPasses(
    RenderGraph& renderGraph,
    Renderer& renderer,
    ResourceContext& resourceContext,
    RenderTargetCache& renderTargetCache,
    const std::string& dependentPass) {
    ImageCache& imageCache = resourceContext.imageCache;
    const VulkanDevice& device = renderer.getDevice();
    FlatHashMap<std::string, std::unique_ptr<RenderNode>> renderNodes;

    const auto createPostProcessingRenderNode2 =
        [&renderNodes, &renderer, &renderGraph, &resourceContext](
            const std::string& renderPassName,
            std::unique_ptr<VulkanRenderPass> renderPass,
            const std::string& luaPipelineFile) {
            renderGraph.addRenderPass(renderPassName, std::move(renderPass));

            auto node = renderNodes.emplace(renderPassName + "Node", std::make_unique<RenderNode>()).first->second.get();
            node->geometry = renderer.getFullScreenGeometry();
            node->pass(renderPassName).pipeline = resourceContext.createPipeline(
                renderPassName + "Pipeline", luaPipelineFile, renderGraph.getRenderPass(renderPassName), 0);
            node->pass(renderPassName).material =
                resourceContext.createMaterial(renderPassName + "Material", node->pass(renderPassName).pipeline);
        };

    resourceContext.createRingBufferFromStruct(
        "atmosphereBuffer", sizeof(AtmosphereParameters), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);

    // Transmittance lookup
    createPostProcessingRenderNode2(
        TransmittanceLutPass, createTransmittanceLutPass(renderer.getDevice(), renderTargetCache), "SkyTransLut.json");
    renderNodes[TransmittanceLutPass + std::string("Node")]
        ->pass(TransmittanceLutPass)
        .material->writeDescriptor(0, 0, *resourceContext.getRingBuffer("atmosphereBuffer"));

    // Multiscattering
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    createInfo.extent = VkExtent3D{32u, 32u, 1u};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 2;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCache.addImage("multiScatTex", std::make_unique<VulkanImage>(renderer.getDevice(), createInfo));
    imageCache.addImageView(
        "multiScatTexView0",
        createView(renderer.getDevice(), imageCache.getImage("multiScatTex"), VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    imageCache.addImageView(
        "multiScatTexView1",
        createView(renderer.getDevice(), imageCache.getImage("multiScatTex"), VK_IMAGE_VIEW_TYPE_2D, 1, 1));

    renderer.enqueueResourceUpdate([tex = &imageCache.getImage("multiScatTex")](VkCommandBuffer cmdBuffer) {
        tex->transitionLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    });

    auto& multiScatPass = renderGraph.addComputePass(MultipleScatteringPass);
    renderGraph.addDependency(TransmittanceLutPass, MultipleScatteringPass, 0);
    multiScatPass.workGroupSize = {1, 1, 64};
    multiScatPass.numWorkGroups = {32, 32, 1};
    multiScatPass.pipeline = createMultiScatPipeline(renderer, multiScatPass.workGroupSize);
    multiScatPass.material = std::make_unique<Material>(multiScatPass.pipeline.get());
    multiScatPass.material->writeDescriptor(0, 0, *resourceContext.getRingBuffer("atmosphereBuffer"));

    std::vector<VulkanImageView*> views{
        &imageCache.getImageView("multiScatTexView0"), &imageCache.getImageView("multiScatTexView1")};
    multiScatPass.material->writeDescriptor(1, 0, views, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    multiScatPass.material->writeDescriptor(
        1, 1, renderGraph.getRenderPass(TransmittanceLutPass), 0, &imageCache.getSampler("linearClamp"));
    multiScatPass.material->setDynamicBufferView(0, *resourceContext.getRingBuffer("atmosphereBuffer"), 0);
    multiScatPass.preDispatchCallback =
        [](RenderGraph::Node& /*node*/, VulkanCommandBuffer& /*cmdBuffer*/, uint32_t /*frameIndex*/) {};

    renderGraph.addRenderPass(SkyViewLutPass, createSkyViewLutPass(renderer.getDevice(), renderTargetCache));
    renderGraph.addDependency(TransmittanceLutPass, SkyViewLutPass, 0);
    renderGraph.addDependency(
        MultipleScatteringPass,
        SkyViewLutPass,
        [tex = &imageCache.getImage("multiScatTex"),
         views](const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t frameIndex) {
            const auto scope = kComputeWrite >> kFragmentRead;
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = scope.srcStage;
            barrier.srcAccessMask = scope.srcAccess;
            barrier.dstStageMask = scope.dstStage;
            barrier.dstAccessMask = scope.dstAccess;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.image = tex->getHandle();
            barrier.subresourceRange = views.at(frameIndex)->getSubresourceRange();
            cmdBuffer.insertImageMemoryBarrier(barrier);
        });

    const auto createPostProcessingRenderNode =
        [&renderNodes, &renderer](const std::string& renderPassName, Material* material, VulkanPipeline* pipeline) {
            auto node = renderNodes.emplace(renderPassName + "Node", std::make_unique<RenderNode>()).first->second.get();
            node->geometry = renderer.getFullScreenGeometry();
            node->pass(renderPassName).material = material;
            node->pass(renderPassName).pipeline = pipeline;
        };

    // Sky View LUT
    auto skyViewLutPipeline =
        resourceContext.createPipeline("skyViewLut", "SkyViewLut.json", renderGraph.getRenderPass(SkyViewLutPass), 0);
    auto skyViewLutMaterial = resourceContext.createMaterial("skyViewLut", skyViewLutPipeline);
    skyViewLutMaterial->writeDescriptor(0, 0, *resourceContext.getRingBuffer("atmosphereBuffer"));
    skyViewLutMaterial->writeDescriptor(
        1, 0, renderGraph.getRenderPass(TransmittanceLutPass), 0, &imageCache.getSampler("linearClamp"));
    skyViewLutMaterial->writeDescriptor(1, 1, views, &imageCache.getSampler("linearClamp"), VK_IMAGE_LAYOUT_GENERAL);
    createPostProcessingRenderNode(SkyViewLutPass, skyViewLutMaterial, skyViewLutPipeline);

    // Camera volumes
    {
        renderGraph.addRenderPass(ViewVolumePass, createSkyVolumePass(device, renderTargetCache));
        renderGraph.addDependency(SkyViewLutPass, ViewVolumePass, 0);
        auto pipeline = resourceContext.createPipeline(
            "skyCameraVolumes", "SkyCameraVolumes.json", renderGraph.getRenderPass(ViewVolumePass), 0);
        auto material = resourceContext.createMaterial("skyCameraVolumes", pipeline);
        material->writeDescriptor(0, 0, *resourceContext.getRingBuffer("atmosphereBuffer"));
        material->writeDescriptor(
            1, 0, renderGraph.getRenderPass(TransmittanceLutPass), 0, &imageCache.getSampler("linearClamp"));
        material->writeDescriptor(1, 1, views, &imageCache.getSampler("linearClamp"), VK_IMAGE_LAYOUT_GENERAL);

        // Voxelized multiple scattering
        const std::vector<glm::vec2> vertices = {{-1.0f, -1.0f}, {+3.0f, -1.0f}, {-1.0f, +3.0f}};
        const std::vector<glm::uvec3> faces = {{0, 2, 1}};
        resourceContext.addGeometry("fullScreenInstanced", Geometry(renderer, vertices, faces));
        resourceContext.getGeometry("fullScreenInstanced")->setInstanceCount(32);

        createPostProcessingRenderNode(ViewVolumePass, material, pipeline);
        renderNodes[ViewVolumePass + std::string("Node")]->geometry = resourceContext.getGeometry(
            "fullScreenInstance"
            "d");
    }

    {
        // Ray marching - final step
        renderGraph.addRenderPass(
            RayMarchingPass,
            createRayMarchingPass(renderer.getDevice(), renderTargetCache, renderer.getSwapChainExtent()));
        renderGraph.addDependency(ViewVolumePass, RayMarchingPass, 0);
        // renderGraph.addDependency(DepthPrePass, RayMarchingPass, 0);
        auto pipeline = resourceContext.createPipeline(
            "rayMarching", "SkyRayMarching.json", renderGraph.getRenderPass(RayMarchingPass), 0);
        auto material = resourceContext.createMaterial("rayMarching", pipeline);
        material->writeDescriptor(0, 0, *resourceContext.getRingBuffer("atmosphereBuffer"));
        material->writeDescriptor(
            1, 0, renderGraph.getRenderPass(TransmittanceLutPass), 0, &imageCache.getSampler("linearClamp"));
        material->writeDescriptor(1, 1, views, &imageCache.getSampler("linearClamp"), VK_IMAGE_LAYOUT_GENERAL);
        material->writeDescriptor(
            1, 2, renderGraph.getRenderPass(SkyViewLutPass), 0, &imageCache.getSampler("linearClamp"));
        material->writeDescriptor(
            1, 3, renderGraph.getRenderPass(ViewVolumePass), 0, &imageCache.getSampler("linearClamp"));
        /*material->writeDescriptor(
            1, 4, renderGraph.getRenderPass(DepthPrePass), 0, &imageCache.getSampler("nearestNeighbor"));*/

        createPostProcessingRenderNode(RayMarchingPass, material, pipeline);

        renderGraph.addDependency(RayMarchingPass, dependentPass, 0);
    }

    return renderNodes;
}
#endif

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

namespace {
constexpr VkExtent3D kMultiScatWorkGroupSize{1, 1, 64};
constexpr uint32_t kMultiScatLutSize = 32;
constexpr uint32_t kCameraVolumeSliceCount = 32;

constexpr const char* kAtmosphereBufferId = "atmosphereBuffer";
constexpr const char* kLinearClampSamplerId = "linearClamp";
constexpr const char* kVolumeGeometryId = "skyCameraVolumesGeometry";

std::unique_ptr<VulkanPipeline> createMultiScatPipeline(Renderer& renderer, const VkExtent3D& workGroupSize) {
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

    return std::make_unique<VulkanPipeline>(device, pipeline, std::move(layout), VK_PIPELINE_BIND_POINT_COMPUTE);
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
        renderGraph.getRenderPass(passName),
        0);
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
        .setInstanceCount(kCameraVolumeSliceCount);

    renderGraph.addPass(
        TransmittanceLutPass,
        [](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<TransmittanceLutData>();
            data.lut = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::Absolute,
                    .width = 256,
                    .height = 64,
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
            // cover the swap chain rather than the 256x64 LUT.
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
                    .width = 32,
                    .height = 32,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                },
                "multiScatTex");

            // The graph does not synchronize a compute pass's outputs, only its inputs, so declaring the storage
            // image as an input of its own producer is what gets it transitioned into GENERAL before the dispatch.
            // Without this it stays in the SHADER_READ_ONLY_OPTIMAL layout its sampled usage selected, which
            // neither matches the storage descriptor nor leaves the tracked layout consistent for the readers.
            builder.readStorageImage(data.tex);
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
                pipeline = createMultiScatPipeline(renderer, kMultiScatWorkGroupSize);
                material = std::make_shared<Material>(pipeline.get());
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
            vkCmdDispatch(cmdBufferHandle, kMultiScatLutSize, kMultiScatLutSize, 1);
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
                    .width = 192,
                    .height = 108,
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
                    .width = 32,
                    .height = 32,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .layerCount = 32,
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
