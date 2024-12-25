
#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphIo.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
namespace {
auto logger = spdlog::stdout_color_st("OceanScene");

constexpr const char* kMainPass = kForwardLightingPass;
constexpr const char* kSpectrumPass = "oscillationPass";
constexpr const char* kGeometryPass = "geometryPass";

constexpr int32_t N = 512;
constexpr int32_t logN = std::bit_width(static_cast<uint32_t>(N)) - 1;
constexpr float Lx = 2.0f * N;
constexpr float kGravity = 9.81f;

constexpr float kGeometryPatchWorldSize = 20.0f;
constexpr uint32_t kInstanceCount = 64;

struct OscillationPassData {
    RenderGraphResourceHandle displacementY;
    RenderGraphResourceHandle displacementX;
    RenderGraphResourceHandle displacementZ;
    RenderGraphResourceHandle normalX;
    RenderGraphResourceHandle normalZ;
};

template <size_t Tag>
struct HorizontalFftPassData {
    std::vector<RenderGraphResourceHandle> image;
};

template <size_t Tag>
struct VerticalFftPassData {
    std::vector<RenderGraphResourceHandle> image;
};

template <size_t Tag>
struct HorizontalBitReversePassData {
    RenderGraphResourceHandle image;
};

template <size_t Tag>
struct VerticalBitReversePassData {
    RenderGraphResourceHandle image;
};

struct GeometryPassData {
    RenderGraphResourceHandle buffer;
};

VkImageMemoryBarrier createImageMemoryReadBarrier(const VkImage imageHandle, const uint32_t layerIdx) {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = imageHandle;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = layerIdx;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    return barrier;
}

struct ComputeDispatch {
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<Material> material;

    glm::vec3 workGroupSize;
    glm::vec3 dispatchSize;

    inline void bind(const RenderPassExecutionContext& ctx) const {
        pipeline->bind(ctx.cmdBuffer.getHandle());
        material->bind(ctx.virtualFrameIndex, ctx.cmdBuffer.getHandle());
    }
};

ComputeDispatch OscillationPassDispatch{};
FlatStringHashMap<ComputeDispatch> BitReversePasses{};
FlatStringHashMap<ComputeDispatch> IfftPasses{};

ComputeDispatch createOscillationPassDispatch(
    Renderer& renderer, const rg::RenderGraph& renderGraph, const ImageCache& imageCache) {
    ComputeDispatch dispatch{};
    dispatch.workGroupSize = glm::ivec3(16, 16, 1);
    dispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N, N, 1), dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(renderer, "ocean-spectrum.comp", dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    auto& opd = renderGraph.getBlackboard().get<OscillationPassData>();
    dispatch.material->writeDescriptor(
        0, 0, imageCache.getImageView("randImageView").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 1, renderGraph.getResourceImageView(opd.displacementY).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 2, renderGraph.getResourceImageView(opd.displacementX).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 3, renderGraph.getResourceImageView(opd.displacementZ).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 4, renderGraph.getResourceImageView(opd.normalX).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 5, renderGraph.getResourceImageView(opd.normalZ).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag>
ComputeDispatch createBitReverseDispatch(
    Renderer& renderer,
    const rg::RenderGraph& renderGraph,
    const VulkanImageView& sourceView,
    const VulkanImageView& dstView) {
    ComputeDispatch dispatch{};
    dispatch.workGroupSize = glm::ivec3(16, 16, 1);
    dispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N, N, 1), dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(renderer, "ocean-reverse-bits.comp", dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    auto& pd = renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>();
    dispatch.material->writeDescriptor(
        0, 0, renderGraph.getResourceImageView(pd.image).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 0, sourceView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 1, dstView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag, bool Horizontal>
ComputeDispatch createIfftDispatch(
    Renderer& renderer,
    const rg::RenderGraph& renderGraph,
    const VulkanImageView& sourceView,
    const VulkanImageView& dstView) {
    constexpr glm::uvec3 workAmount = Horizontal ? glm::uvec3(N / 2, N, 1) : glm::uvec3(N, N / 2, 1);
    const std::string shaderName = Horizontal ? "ifft-hori.comp" : "ifft-vert.comp";

    ComputeDispatch dispatch{};
    dispatch.workGroupSize = glm::ivec3(16, 16, 1);
    dispatch.dispatchSize = computeWorkGroupCount(workAmount, dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(renderer, shaderName, dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    auto& pd = renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>();
    dispatch.material->writeDescriptor(
        0, 0, renderGraph.getResourceImageView(pd.image).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 0, sourceView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 1, dstView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag>
void createFftDispatches(Renderer& renderer, const rg::RenderGraph& renderGraph, const VulkanImageView& srcView) {
    BitReversePasses[fmt::format("bit-reverse-h-{}", Tag)] = createBitReverseDispatch<Tag>(
        renderer,
        renderGraph,
        srcView,
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image));
    // for (int32_t i = 0; i < logN; ++i) {
    //     if (i == 0) {
    //         IfftPasses[fmt::format("ifft-h-{}-{}", Tag, i)] = createIfftDispatch<Tag, true>(
    //             renderer,
    //             renderGraph,
    //             renderGraph.getResourceImageView(
    //                 renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image),
    //             renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i]));
    //     } else {
    //         IfftPasses[fmt::format("ifft-h-{}-{}", Tag, i)] = createIfftDispatch<Tag, true>(
    //             renderer,
    //             renderGraph,
    //             renderGraph.getResourceImageView(
    //                 renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i - 1]),
    //             renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i]));
    //     }
    // }

    // BitReversePasses[fmt::format("bit-reverse-v-{}", Tag)] = createBitReverseDispatch<Tag>(
    //     renderer,
    //     renderGraph,
    //     renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image.back()),
    //     renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalBitReversePassData<Tag>>().image));
    // for (int32_t i = 0; i < logN; ++i) {
    //     if (i == 0) {
    //         IfftPasses[fmt::format("ifft-v-{}-{}", Tag, i)] = createIfftDispatch<Tag, false>(
    //             renderer,
    //             renderGraph,
    //             renderGraph.getResourceImageView(
    //                 renderGraph.getBlackboard().get<VerticalBitReversePassData<Tag>>().image),
    //             renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i]));
    //     } else {
    //         IfftPasses[fmt::format("ifft-v-{}-{}", Tag, i)] = createIfftDispatch<Tag, false>(
    //             renderer,
    //             renderGraph,
    //             renderGraph.getResourceImageView(
    //                 renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i - 1]),
    //             renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i]));
    //     }
    // }
}

} // namespace

OceanScene::OceanScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    , m_oceanParams(createOceanParameters(N, Lx, 10.0f, 0.0f, 4.0f, 0.5f))
    , m_choppiness(0.0f) {
    m_window->keyPressed += [this](Key key, int /*modifiers*/) {
        if (key == Key::Space) {
            m_paused = !m_paused;
        } else if (key == Key::F5) {
            m_resourceContext->recreatePipelines();
        }
    };

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
        {VertexAttribute::Position}, {VertexAttribute::Normal}};
    TriangleMesh mesh = createGridMesh(kGeometryPatchWorldSize, N);
    m_resourceContext->addGeometry(
        "ocean", createFromMesh(*m_renderer, mesh, vertexFormat, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    m_resourceContext->getGeometry("ocean")->setInstanceCount(kInstanceCount);

    auto displacementYImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto displacementXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto displacementZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto normalXImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto normalZImage = createStorageImage(m_renderer->getDevice(), 2, N, N, VK_FORMAT_R32G32_SFLOAT);
    auto spectrumImage = createInitialSpectrum();
    m_resourceContext->imageCache.addImageView(
        "randImageView", createView(m_renderer->getDevice(), *spectrumImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    m_resourceContext->imageCache.addImage("randImage", std::move(spectrumImage));

    buildNewFFT();

    // m_renderer->enqueueResourceUpdate(
    //     [dy = displacementYImage.get(),
    //      dx = displacementXImage.get(),
    //      dz = displacementZImage.get(),
    //      nx = normalXImage.get(),
    //      nz = normalZImage.get()](VkCommandBuffer cmdBuffer) {
    //         const auto transitionToGeneral = [cmdBuffer](VulkanImage& image) {
    //             image.transitionLayout(
    //                 cmdBuffer,
    //                 VK_IMAGE_LAYOUT_GENERAL,
    //                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    //         };
    //         transitionToGeneral(*dy);
    //         transitionToGeneral(*dx);
    //         transitionToGeneral(*dz);
    //         transitionToGeneral(*nx);
    //         transitionToGeneral(*nz);
    //     });

    // auto& imageCache = m_resourceContext->imageCache;
    // const auto addImage = [&imageCache](std::unique_ptr<VulkanImage> image, const std::string& name) {
    //     for (uint32_t i = 0; i < 2; ++i) {
    //         const std::string viewName{fmt::format("{}View{}", name, i)};
    //         imageCache.addImageView(viewName, createView(*image, VK_IMAGE_VIEW_TYPE_2D, i, 1));
    //     }
    //     imageCache.addImage(name, std::move(image));
    // };
    // addImage(std::move(displacementYImage), "fftImage");
    // addImage(std::move(displacementXImage), "displacementX");
    // addImage(std::move(displacementZImage), "displacementZ");
    // addImage(std::move(normalXImage), "normalX");
    // addImage(std::move(normalZImage), "normalZ");

    // imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), MaxAnisotropy));
    // imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy));
    // imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), MaxAnisotropy, 9.0f));

    // auto& oscillationPass = m_renderGraph->addComputePass(kSpectrumPass);
    // oscillationPass.workGroupSize = glm::ivec3(16, 16, 1);
    // oscillationPass.numWorkGroups = computeWorkGroupCount(glm::uvec3(N, N, 1), oscillationPass.workGroupSize);
    // oscillationPass.pipeline = createComputePipeline(*m_renderer, "ocean-spectrum.comp",
    // oscillationPass.workGroupSize); oscillationPass.material =
    // std::make_unique<Material>(oscillationPass.pipeline.get()); oscillationPass.material->writeDescriptor(
    //     0, 0, imageCache.getImageView("randImageView").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // oscillationPass.material->writeDescriptor(
    //     0, 1, imageCache.getImageView("fftImageView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // oscillationPass.material->writeDescriptor(
    //     0, 2, imageCache.getImageView("displacementXView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // oscillationPass.material->writeDescriptor(
    //     0, 3, imageCache.getImageView("displacementZView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // oscillationPass.material->writeDescriptor(
    //     0, 4, imageCache.getImageView("normalXView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    // oscillationPass.material->writeDescriptor(
    //     0, 5, imageCache.getImageView("normalZView0").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

    // oscillationPass.preDispatchCallback =
    //     [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
    //         VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    //         barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    //         barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
    //         barrier.offset = 0;
    //         barrier.size = VK_WHOLE_SIZE;

    //         vkCmdPipelineBarrier(
    //             cmdBuffer.getHandle(),
    //             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    //             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //             0,
    //             0,
    //             nullptr,
    //             1,
    //             &barrier,
    //             0,
    //             nullptr);

    //         node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);
    //     };

    // int layerToRead = applyFFT("fftImage");
    // int ltr1 = applyFFT("displacementX");
    // int ltr2 = applyFFT("displacementZ");

    // int ln1 = applyFFT("normalX");
    // int ln2 = applyFFT("normalZ");

    // auto& geometryPass = m_renderGraph->addComputePass(kGeometryPass);
    // geometryPass.workGroupSize = glm::ivec3(16, 16, 1);
    // geometryPass.numWorkGroups = computeWorkGroupCount(glm::uvec3(N + 1, N + 1, 1), geometryPass.workGroupSize);
    // geometryPass.pipeline = createComputePipeline(*m_renderer, "ocean-geometry.comp", geometryPass.workGroupSize);
    // geometryPass.material = std::make_unique<Material>(geometryPass.pipeline.get());
    // geometryPass.material->writeDescriptor(
    //     0, 0, m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->createDescriptorInfo());
    // geometryPass.material->writeDescriptor(
    //     0, 1, m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->createDescriptorInfo());
    // geometryPass.material->writeDescriptor(
    //     0,
    //     2,
    //     imageCache.getImageView("fftImageView" + std::to_string(layerToRead))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // geometryPass.material->writeDescriptor(
    //     0,
    //     3,
    //     imageCache.getImageView("displacementXView" + std::to_string(ltr1))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // geometryPass.material->writeDescriptor(
    //     0,
    //     4,
    //     imageCache.getImageView("displacementZView" + std::to_string(ltr2))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // geometryPass.material->writeDescriptor(
    //     0,
    //     5,
    //     imageCache.getImageView("normalXView" + std::to_string(ln1))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // geometryPass.material->writeDescriptor(
    //     0,
    //     6,
    //     imageCache.getImageView("normalZView" + std::to_string(ln2))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));

    // geometryPass.preDispatchCallback =
    //     [this](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
    //         std::array<VkBufferMemoryBarrier, 2> barriers{};
    //         barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //         barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //         barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    //         barriers[0].buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->getHandle();
    //         barriers[0].offset = 0;
    //         barriers[0].size = VK_WHOLE_SIZE;

    //         barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    //         barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //         barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    //         barriers[1].buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->getHandle();
    //         barriers[1].offset = 0;
    //         barriers[1].size = VK_WHOLE_SIZE;

    //         vkCmdPipelineBarrier(
    //             cmdBuffer.getHandle(),
    //             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    //             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //             0,
    //             0,
    //             nullptr,
    //             2,
    //             barriers.data(),
    //             0,
    //             nullptr);

    //         struct GeometryUpdateParams {
    //             int patchSize{N};
    //             float patchWorldSize{};
    //             float choppiness{};
    //         };

    //         GeometryUpdateParams params{};
    //         params.patchWorldSize = kGeometryPatchWorldSize;
    //         params.choppiness = m_choppiness;
    //         node.pipeline->setPushConstants(cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, params);
    //     };

    // m_renderGraph->addRenderPass(
    //     kMainPass,
    //     createForwardLightingPass(
    //         m_renderer->getDevice(), m_resourceContext->renderTargetCache, renderer->getSwapChainExtent()));

    // m_renderGraph->addDependency(
    //     kGeometryPass,
    //     kMainPass,
    //     [this](const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
    //         VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    //         barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    //         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //         barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
    //         barrier.offset = 0;
    //         barrier.size = VK_WHOLE_SIZE;
    //         cmdBuffer.insertBufferMemoryBarrier(
    //             barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

    //         barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(1)->getHandle();
    //         barrier.offset = 0;
    //         barrier.size = VK_WHOLE_SIZE;
    //         cmdBuffer.insertBufferMemoryBarrier(
    //             barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
    //     });

    // m_renderGraph->addDependency(kMainPass, "SCREEN", 0);
    // m_renderGraph->sortRenderPasses().unwrap();
    // m_renderGraph->printExecutionOrder();
    // m_renderer->setSceneImageView(m_renderGraph->getNode(kMainPass).renderPass.get(), 0);

    // m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 200);

    // m_envLight = std::make_unique<EnvironmentLight>(
    //     *m_renderer,
    //     loadImageBasedLightingData(m_renderer->getResourcesPath() /
    //     "Textures/EnvironmentMaps/TableMountain").unwrap());
    // m_skybox = m_envLight->createSkybox(
    //     *m_renderer, m_renderGraph->getRenderPass(kMainPass), imageCache.getSampler("linearClamp"));
    // m_renderer->flushResourceUpdates(true);

    // VulkanPipeline* pipeline =
    //     m_resourceContext->createPipeline("ocean", "ocean.json", m_renderGraph->getRenderPass(kMainPass), 0);
    // Material* material = m_resourceContext->createMaterial("ocean", pipeline);
    // material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    // material->writeDescriptor(
    //     0,
    //     1,
    //     imageCache.getImageView("fftImageView" + std::to_string(layerToRead))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // material->writeDescriptor(
    //     0,
    //     2,
    //     imageCache.getImageView("displacementXView" + std::to_string(ltr1))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // material->writeDescriptor(
    //     0,
    //     3,
    //     imageCache.getImageView("displacementZView" + std::to_string(ltr2))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));

    // material->writeDescriptor(
    //     0,
    //     4,
    //     imageCache.getImageView("normalXView" + std::to_string(ln1))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // material->writeDescriptor(
    //     0,
    //     5,
    //     imageCache.getImageView("normalZView" + std::to_string(ln2))
    //         .getDescriptorInfo(&imageCache.getSampler("linearRepeat"), VK_IMAGE_LAYOUT_GENERAL));
    // material->writeDescriptor(1, 0, *m_resourceContext->getUniformBuffer("camera"));
    // material->writeDescriptor(1, 1, m_envLight->getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    // material->writeDescriptor(1, 2, m_envLight->getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    // imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    // material->writeDescriptor(1, 3, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    // auto node = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    // node->transformPack->M = glm::mat4(1.0f);
    // node->geometry = m_resourceContext->getGeometry("ocean");
    // node->pass(kMainPass).material = material;
    // node->pass(kMainPass).setPushConstants(kGeometryPatchWorldSize);
    // m_renderNodes.emplace_back(std::move(node));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void OceanScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(width, height);
    m_renderer->setSceneImageView(m_renderGraph->getNode(kMainPass).renderPass.get(), 0);
}

void OceanScene::update(float dt) {
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();

    // m_transformBuffer->update(cameraParams.V, cameraParams.P);
    // m_skybox->updateTransforms(cameraParams.V, cameraParams.P);

    // m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);

    // if (!m_paused) {
    //     m_oceanParams.time += dt;
    // }
}

void OceanScene::render() {
    m_renderer->enqueueDrawCommand([this](const VkCommandBuffer cmdBuffer) {
        m_rg->execute(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex(), m_renderer->getDevice().getHandle());
    });

    // m_renderGraph->clearCommandLists();
    // m_renderGraph->buildCommandLists(m_renderNodes);
    // m_renderGraph->addToCommandLists(m_skybox->getRenderNode());
    // m_renderGraph->executeCommandLists();
}

void OceanScene::renderGui() {
    ImGui::Begin("Ocean Parameters");
    glm::vec2 windVelocity = m_oceanParams.windDirection * m_oceanParams.windSpeed;
    if (ImGui::SliderFloat("Wind Speed X", &windVelocity.x, 0.001f, 100.0f)) {
        m_oceanParams.windSpeed = glm::length(windVelocity);
        m_oceanParams.windDirection = windVelocity / m_oceanParams.windSpeed;
        m_oceanParams.Lw = m_oceanParams.windSpeed * m_oceanParams.windSpeed / kGravity;
    }
    if (ImGui::SliderFloat("Wind Speed Z", &windVelocity.y, 0.001f, 100.0f)) {
        m_oceanParams.windSpeed = glm::length(windVelocity);
        m_oceanParams.windDirection = windVelocity / m_oceanParams.windSpeed;
        m_oceanParams.Lw = m_oceanParams.windSpeed * m_oceanParams.windSpeed / kGravity;
    }
    ImGui::SliderFloat("Amplitude", &m_oceanParams.A, 0.0f, 1000.0f);
    ImGui::SliderFloat("Small Waves", &m_oceanParams.smallWaves, 0.0f, 10.0f);
    ImGui::SliderFloat("Choppiness", &m_choppiness, 0.0f, 10.0f);
    ImGui::End();
}

std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum() {
    const auto oceanSpectrum{createOceanSpectrum(0, m_oceanParams)};

    auto image = createStorageImage(m_renderer->getDevice(), 1, N, N, VK_FORMAT_R32G32B32A32_SFLOAT);
    std::shared_ptr<VulkanBuffer> buffer = createStagingBuffer(m_renderer->getDevice(), oceanSpectrum);
    m_renderer->enqueueResourceUpdate([img = image.get(), stagingBuffer = buffer](VkCommandBuffer cmdBuffer) {
        img->transitionLayout(
            cmdBuffer,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        img->copyFrom(cmdBuffer, *stagingBuffer, 0, 1);

        img->transitionLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    });

    return image;
}

int OceanScene::applyFFT(std::string image) {
    struct BitReversalPushConstants {
        int32_t reversalDirection; // 0 is horizontal, 1 is vertical.
        int32_t passCount;         // Logarithm of the patch discretization size.
    };

    struct IFFTPushConstants {
        int32_t passIdx; // Index of the current pass.
        int32_t N;       // Number of elements in the array.
    };

    auto& imageCache = m_resourceContext->imageCache;
    int32_t imageLayerRead = 0;
    int32_t imageLayerWrite = 1;
    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        auto& bitReversePass = m_renderGraph->addComputePass(image + "BitReversePass0");
        bitReversePass.workGroupSize = glm::ivec3(16, 16, 1);
        bitReversePass.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
        bitReversePass.pipeline =
            createComputePipeline(*m_renderer, "ocean-reverse-bits.comp", bitReversePass.workGroupSize);
        bitReversePass.material = std::make_unique<Material>(bitReversePass.pipeline.get());
        bitReversePass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        bitReversePass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        bitReversePass.preDispatchCallback =
            [image](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                node.pipeline->setPushConstants(
                    cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{0, logN});
            };
        m_renderGraph->addDependency(
            kSpectrumPass,
            image + "BitReversePass0",
            [this, image, imageLayerRead](
                const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                cmdBuffer.insertImageMemoryBarrier(
                    createImageMemoryReadBarrier(
                        m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });
        CRISP_LOGI("{} R: {} W: {}", image + "BitReversePass0", imageLayerRead, imageLayerWrite);

        std::swap(imageLayerRead, imageLayerWrite);
    }

    // Horizontal IFFT passes.
    for (int i = 0; i < logN; ++i) {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        std::string name = image + "TrueFFTPass" + std::to_string(i);
        auto& fftPass = m_renderGraph->addComputePass(name);
        fftPass.workGroupSize = glm::ivec3(16, 16, 1);
        fftPass.numWorkGroups = glm::ivec3(N / 2 / 16, N / 16, 1);
        fftPass.pipeline = createComputePipeline(*m_renderer, "ifft-hori.comp", fftPass.workGroupSize);
        fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
        fftPass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        fftPass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        fftPass.preDispatchCallback =
            [i](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                node.pipeline->setPushConstants(
                    cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
            };

        CRISP_LOGI("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

        if (i == 0) {
            m_renderGraph->addDependency(
                image + "BitReversePass0",
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        if (i > 0) {
            std::string prevName = image + "TrueFFTPass" + std::to_string(i - 1);
            m_renderGraph->addDependency(
                prevName,
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        std::swap(imageLayerRead, imageLayerWrite);
    }

    {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);
        auto& bitReversePass2 = m_renderGraph->addComputePass(image + "BitReversePass1");
        bitReversePass2.workGroupSize = glm::ivec3(16, 16, 1);
        bitReversePass2.numWorkGroups = glm::ivec3(N / 16, N / 16, 1);
        bitReversePass2.pipeline =
            createComputePipeline(*m_renderer, "ocean-reverse-bits.comp", bitReversePass2.workGroupSize);
        bitReversePass2.material = std::make_unique<Material>(bitReversePass2.pipeline.get());
        bitReversePass2.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        bitReversePass2.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        bitReversePass2.preDispatchCallback =
            [image](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                node.pipeline->setPushConstants(
                    cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{1, logN});
            };

        CRISP_LOGI("{} R: {} W: {}", image + "BitReversePass1", imageLayerRead, imageLayerWrite);

        m_renderGraph->addDependency(
            image + "TrueFFTPass" + std::to_string(logN - 1),
            image + "BitReversePass1",
            [this, image, imageLayerRead](
                const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                cmdBuffer.insertImageMemoryBarrier(
                    createImageMemoryReadBarrier(
                        m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            });
        std::swap(imageLayerRead, imageLayerWrite);
    }

    // Vertical IFFT pass.
    std::string finalImageView;
    for (int i = 0; i < logN; ++i) {
        const std::string imageViewRead = fmt::format("{}View{}", image, imageLayerRead);
        const std::string imageViewWrite = fmt::format("{}View{}", image, imageLayerWrite);

        std::string name = image + "TrueFFTPassVert" + std::to_string(i);
        auto& fftPass = m_renderGraph->addComputePass(name);
        fftPass.workGroupSize = glm::ivec3(16, 16, 1);
        fftPass.numWorkGroups = glm::ivec3(N / 16, N / 16 / 2, 1);
        fftPass.pipeline = createComputePipeline(*m_renderer, "ifft-vert.comp", fftPass.workGroupSize);
        fftPass.material = std::make_unique<Material>(fftPass.pipeline.get());
        fftPass.material->writeDescriptor(
            0, 0, imageCache.getImageView(imageViewRead).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
        fftPass.material->writeDescriptor(
            0, 1, imageCache.getImageView(imageViewWrite).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));

        CRISP_LOGI("{} R: {} W: {}", name, imageLayerRead, imageLayerWrite);

        fftPass.preDispatchCallback =
            [i](RenderGraph::Node& node, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                node.pipeline->setPushConstants(
                    cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
            };

        if (i == logN - 1) {
            finalImageView = imageViewWrite;
            m_renderGraph->addDependency(
                name,
                kGeometryPass,
                [this, image, imageLayerWrite](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.image = m_resourceContext->imageCache.getImage(image).getHandle();
                    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseArrayLayer = imageLayerWrite;
                    barrier.subresourceRange.layerCount = 1;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    cmdBuffer.insertImageMemoryBarrier(
                        barrier, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
                });
        } else if (i == 0) {
            m_renderGraph->addDependency(
                image + "BitReversePass1",
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        if (i > 0) {
            std::string prevName = image + "TrueFFTPassVert" + std::to_string(i - 1);
            m_renderGraph->addDependency(
                prevName,
                name,
                [this, image, imageLayerRead](
                    const VulkanRenderPass&, VulkanCommandBuffer& cmdBuffer, uint32_t /*frameIndex*/) {
                    cmdBuffer.insertImageMemoryBarrier(
                        createImageMemoryReadBarrier(
                            m_resourceContext->imageCache.getImage(image).getHandle(), imageLayerRead),
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
                });
        }

        std::swap(imageLayerRead, imageLayerWrite);
    }

    return imageLayerRead;
}

void OceanScene::buildNewFFT() {
    m_rg = std::make_unique<rg::RenderGraph>();
    m_rg->getBlackboard().insert<OscillationPassData>();
    m_rg->addPass(
        "oscillation",
        [](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
            auto& data = builder.getBlackboard().get<OscillationPassData>();
            data.displacementY = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-disp-y", "oscillation"));
            data.displacementX = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-disp-x", "oscillation"));
            data.displacementZ = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-disp-z", "oscillation"));
            data.normalX = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-normal-x", "oscillation"));
            data.normalZ = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-normal-z", "oscillation"));
        },
        [this](const RenderPassExecutionContext& ctx) {
            OscillationPassDispatch.bind(ctx);
            OscillationPassDispatch.pipeline->setPushConstants(
                ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);

            VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                ctx.cmdBuffer.getHandle(),
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                1,
                &barrier,
                0,
                nullptr);

            ctx.cmdBuffer.dispatchCompute(OscillationPassDispatch.dispatchSize);
        });

    auto addFftPasses = [this]<size_t Tag>(const RenderGraphResourceHandle image) {
        struct BitReversalPushConstants {
            int32_t reversalDirection; // 0 is horizontal, 1 is vertical.
            int32_t passCount;         // Logarithm of the patch discretization size.
        };

        const std::string bitReversePassHorName{fmt::format("bit-reverse-h-{}", Tag)};
        m_rg->addPass(
            bitReversePassHorName,
            [image, bitReversePassHorName](rg::RenderGraph::Builder& builder) {
                builder.setType(PassType::Compute);
                builder.readStorageImage(image);
                auto& data = builder.getBlackboard().insert<HorizontalBitReversePassData<Tag>>();
                data.image = builder.createStorageImage(
                    {
                        .sizePolicy = SizePolicy::Absolute,
                        .width = N,
                        .height = N,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                    },
                    fmt::format("{}-image", bitReversePassHorName));
            },
            [](const RenderPassExecutionContext& ctx) {
                const auto& dispatch{BitReversePasses.at(fmt::format("bit-reverse-h-{}", Tag))};
                dispatch.bind(ctx);
                dispatch.pipeline->setPushConstants(
                    ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{0, logN});
                ctx.cmdBuffer.dispatchCompute(dispatch.dispatchSize);
            });

        // struct IFFTPushConstants {
        //     int32_t passIdx; // Index of the current pass.
        //     int32_t N;       // Number of elements in the array.
        // };
        // for (int i = 0; i < logN; ++i) {

        //     const std::string passName = fmt::format("ifft-h-{}-{}", Tag, i);
        //     m_rg->addPass(
        //         passName,
        //         [passName, i](rg::RenderGraph::Builder& builder) {
        //             builder.setType(PassType::Compute);
        //             auto& data =
        //                 i == 0 ? builder.getBlackboard().insert<HorizontalFftPassData<Tag>>()
        //                        : builder.getBlackboard().get<HorizontalFftPassData<Tag>>();
        //             if (i == 0) {
        //                 data.image.resize(logN);
        //                 builder.readStorageImage(builder.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image);
        //             } else {
        //                 builder.readStorageImage(data.image[i - 1]);
        //             }
        //             data.image[i] = builder.createStorageImage(
        //                 {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format =
        //                 VK_FORMAT_R32G32_SFLOAT}, fmt::format("{}-image", passName));
        //         },
        //         [i](const RenderPassExecutionContext& ctx) {
        //             const auto& dispatch{IfftPasses.at(fmt::format("ifft-h-{}-{}", Tag, i))};
        //             dispatch.bind(ctx);
        //             dispatch.pipeline->setPushConstants(
        //                 ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
        //             ctx.cmdBuffer.dispatchCompute(dispatch.dispatchSize);
        //         });
        // }

        // const std::string bitReversePassVertName{fmt::format("bit-reverse-v-{}", Tag)};
        // m_rg->addPass(
        //     bitReversePassVertName,
        //     [bitReversePassVertName](rg::RenderGraph::Builder& builder) {
        //         builder.setType(PassType::Compute);
        //         builder.readStorageImage(builder.getBlackboard().get<HorizontalFftPassData<Tag>>().image.back());
        //         auto& data = builder.getBlackboard().insert<VerticalBitReversePassData<Tag>>();
        //         data.image = builder.createStorageImage(
        //             {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
        //             fmt::format("{}-image", bitReversePassVertName));
        //     },
        //     [](const RenderPassExecutionContext& ctx) {
        //         const auto& dispatch{BitReversePasses.at(fmt::format("bit-reverse-v-{}", Tag))};
        //         dispatch.bind(ctx);
        //         dispatch.pipeline->setPushConstants(
        //             ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, BitReversalPushConstants{1, logN});
        //         ctx.cmdBuffer.dispatchCompute(dispatch.dispatchSize);
        //     });

        // for (int i = 0; i < logN; ++i) {
        //     const std::string passName = fmt::format("ifft-v-{}-{}", Tag, i);
        //     m_rg->addPass(
        //         passName,
        //         [passName, i](rg::RenderGraph::Builder& builder) {
        //             builder.setType(PassType::Compute);
        //             auto& data =
        //                 i == 0 ? builder.getBlackboard().insert<VerticalFftPassData<Tag>>()
        //                        : builder.getBlackboard().get<VerticalFftPassData<Tag>>();
        //             if (i == 0) {
        //                 data.image.resize(logN);
        //                 builder.readStorageImage(builder.getBlackboard().get<VerticalBitReversePassData<Tag>>().image);
        //             } else {
        //                 builder.readStorageImage(data.image[i - 1]);
        //             }
        //             data.image[i] = builder.createStorageImage(
        //                 {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format =
        //                 VK_FORMAT_R32G32_SFLOAT}, fmt::format("{}-image", passName));
        //         },
        //         [i](const RenderPassExecutionContext& ctx) {
        //             const auto& dispatch{IfftPasses.at(fmt::format("ifft-v-{}-{}", Tag, i))};
        //             dispatch.bind(ctx);
        //             dispatch.pipeline->setPushConstants(
        //                 ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, IFFTPushConstants{i + 1, N});
        //             ctx.cmdBuffer.dispatchCompute(dispatch.dispatchSize);
        //         });
        // }
    };
    addFftPasses.operator()<0>(m_rg->getBlackboard().get<OscillationPassData>().displacementY);
    // addFftPasses.operator()<1>(m_rg->getBlackboard().get<OscillationPassData>().displacementX);
    // addFftPasses.operator()<2>(m_rg->getBlackboard().get<OscillationPassData>().displacementZ);
    // addFftPasses.operator()<3>(m_rg->getBlackboard().get<OscillationPassData>().normalX);
    // addFftPasses.operator()<4>(m_rg->getBlackboard().get<OscillationPassData>().normalZ);

    m_rg->addPass(
        "geometry",
        [this](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
            builder.readTexture(builder.getBlackboard().get<HorizontalBitReversePassData<0>>().image);
            // builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<0>>().image.back());
            // builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image.back());
            // builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<2>>().image.back());
            // builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<3>>().image.back());
            // builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<4>>().image.back());

            auto& data = builder.getBlackboard().insert<GeometryPassData>();
            data.buffer = builder.createBuffer(
                {
                    .formatHint = VK_FORMAT_R8_UINT,
                    .size = 100,
                    .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                },
                "dummy-geometry");

            // builder.importBuffer(
            //     {
            //         .formatHint = VK_FORMAT_R32G32B32_SFLOAT,
            //         .size = m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->getSize(),
            //         .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            //         .externalBuffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer(0)->getHandle(),
            //     },
            //     "geometry-positions");
        },
        [this](const RenderPassExecutionContext& ctx) {
            // VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            // barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            // barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            // barrier.buffer = m_resourceContext->getGeometry("ocean")->getVertexBuffer()->getHandle();
            // barrier.offset = 0;
            // barrier.size = VK_WHOLE_SIZE;

            // vkCmdPipelineBarrier(
            //     ctx.cmdBuffer.getHandle(),
            //     VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            //     0,
            //     0,
            //     nullptr,
            //     1,
            //     &barrier,
            //     0,
            //     nullptr);

            // node.pipeline->setPushConstants(ctx.cmdBuffer.getHandle(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);
        });

    m_rg->addPass(
        kForwardLightingPass,
        [](rg::RenderGraph::Builder& builder) {
            builder.readBuffer(builder.getBlackboard().get<GeometryPassData>().buffer);
            auto& data = builder.getBlackboard().insert<ForwardLightingData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kForwardLightingPass),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});

            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", kForwardLightingPass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        [this](const RenderPassExecutionContext& ctx) {});

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
    });
    m_renderer->flushResourceUpdates(true);
    toGraphViz(*m_rg, "D:/graph.dot").unwrap();

    OscillationPassDispatch = createOscillationPassDispatch(*m_renderer, *m_rg, m_resourceContext->imageCache);
    createFftDispatches<0>(
        *m_renderer, *m_rg, m_rg->getResourceImageView(m_rg->getBlackboard().get<OscillationPassData>().displacementY));
    // createFftDispatches<1>(
    //     *m_renderer, *m_rg,
    //     m_rg->getResourceImageView(m_rg->getBlackboard().get<OscillationPassData>().displacementX));
    // createFftDispatches<2>(
    //     *m_renderer, *m_rg,
    //     m_rg->getResourceImageView(m_rg->getBlackboard().get<OscillationPassData>().displacementZ));
    // createFftDispatches<3>(
    //     *m_renderer, *m_rg, m_rg->getResourceImageView(m_rg->getBlackboard().get<OscillationPassData>().normalX));
    // createFftDispatches<4>(
    //     *m_renderer, *m_rg, m_rg->getResourceImageView(m_rg->getBlackboard().get<OscillationPassData>().normalZ));
}

} // namespace crisp