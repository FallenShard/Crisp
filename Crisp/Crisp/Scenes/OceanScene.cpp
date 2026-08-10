
#include <Crisp/Scenes/OceanScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/ComputePipeline.hpp>
#include <Crisp/Renderer/PipelineBuilder.hpp>
#include <Crisp/Renderer/PipelineLayoutBuilder.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphIo.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>
#include <Crisp/Vulkan/VulkanCommandEncoder.hpp>
#include <Crisp/Vulkan/VulkanStagingBuffer.hpp>

namespace crisp {
namespace {
constexpr int32_t N = 512;
constexpr int32_t logN = std::bit_width(static_cast<uint32_t>(N)) - 1;
constexpr float kGravity = 9.81f;

// Also the L in the spectral grid k = 2 * pi * n / L, so spectrum and geometry must agree on it.
constexpr float kPatchWorldSize = 256.0f;
constexpr float kCellSize = kPatchWorldSize / N;

constexpr int32_t kMaxInstancesPerSide = 8;

// height/dispX and dispZ/normalX are each packed into one complex FFT channel (see
// ocean-spectrum.comp.glsl); normalZ is left unpaired. 5 logical fields, 3 FFT channels.
struct OscillationPassData {
    RenderGraphResourceHandle packedHeightDispX;
    RenderGraphResourceHandle packedDispZNormalX;
    RenderGraphResourceHandle normalZ;
};

// Shared-memory IFFT: one dispatch per direction folds the bit-reversal and all logN butterfly
// stages into a single pass (a workgroup owns a whole row/column in `shared` memory), rather than
// 1 + logN separate dispatches each round-tripping through VRAM.
template <size_t Tag>
struct HorizontalFftPassData {
    RenderGraphResourceHandle image;
};

template <size_t Tag>
struct VerticalFftPassData {
    RenderGraphResourceHandle image;
};

struct GeometryPassData {
    RenderGraphResourceHandle positions;
    RenderGraphResourceHandle normals;
};

struct OceanOutputData {
    RenderGraphResourceHandle hdrImage;
};

struct ComputeDispatch {
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<Material> material;

    VkExtent3D workGroupSize;
    VkExtent3D dispatchSize;

    void bind(const FrameContext& ctx) const {
        ctx.commandEncoder.bindPipeline(*pipeline);
        ctx.commandEncoder.bindDescriptorSets(material->getDescriptorSetBinding());
    }
};

} // namespace

struct OceanPassResources {
    ComputeDispatch oscillation;
    ComputeDispatch geometry;
    FlatStringHashMap<ComputeDispatch> ifft;
};

namespace {

ComputeDispatch createOscillationPassDispatch(
    Renderer& renderer, const rg::RenderGraph& renderGraph, const ImageCache& imageCache) {
    ComputeDispatch dispatch{};
    dispatch.workGroupSize = {16, 16, 1};
    dispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N, N, 1), dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(renderer, "ocean-spectrum.comp", dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    auto& opd = renderGraph.getBlackboard().get<OscillationPassData>();
    dispatch.material->writeDescriptor(
        0, 0, imageCache.getImageView("randImageView").getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0,
        1,
        renderGraph.getResourceImageView(opd.packedHeightDispX).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0,
        2,
        renderGraph.getResourceImageView(opd.packedDispZNormalX).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(
        0, 3, renderGraph.getResourceImageView(opd.normalZ).getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag, bool Horizontal>
ComputeDispatch createSharedIfftDispatch(
    Renderer& renderer, const VulkanImageView& sourceView, const VulkanImageView& dstView) {
    const std::string shaderName = Horizontal ? "ifft-shared-hori.comp" : "ifft-shared-vert.comp";

    ComputeDispatch dispatch{};
    // One workgroup per row (horizontal) or column (vertical); N/2 threads butterfly the whole
    // line in shared memory, so the dispatch is 1-wide in the direction being transformed.
    dispatch.workGroupSize = {static_cast<uint32_t>(N / 2), 1, 1};
    dispatch.dispatchSize =
        Horizontal ? VkExtent3D{1, static_cast<uint32_t>(N), 1} : VkExtent3D{static_cast<uint32_t>(N), 1, 1};
    dispatch.pipeline = createComputePipeline(renderer, shaderName, dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->writeDescriptor(0, 0, sourceView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 1, dstView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag>
void createFftDispatches(
    OceanPassResources& passResources,
    Renderer& renderer,
    const rg::RenderGraph& renderGraph,
    const VulkanImageView& srcView) {
    passResources.ifft[fmt::format("ifft-h-{}", Tag)] = createSharedIfftDispatch<Tag, true>(
        renderer, srcView, renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image));
    passResources.ifft[fmt::format("ifft-v-{}", Tag)] = createSharedIfftDispatch<Tag, false>(
        renderer,
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image),
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image));
}

} // namespace

OceanScene::OceanScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    // A is the Phillips constant; at 10 m/s this gives an 85 m peak wavelength and H_s ~ 2.3 m.
    , m_oceanParams(createOceanParameters(N, kPatchWorldSize, 10.0f, 0.0f, 0.001f, kCellSize))
    , m_choppiness(1.0f) {
    setupInput();
    setupResources();
    buildRenderGraph();

    m_renderer->getDevice().flushDescriptorUpdates();
}

void OceanScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](Key key, int /*modifiers*/) {
        if (key == Key::Space) {
            m_paused = !m_paused;
        } else if (key == Key::F5) {
            m_resourceContext->recreatePipelines();
        }
    }));
}

void OceanScene::setupResources() {
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    std::vector<std::vector<VertexAttributeDescriptor>> vertexFormat = {
        {VertexAttribute::Position}, {VertexAttribute::Normal}};
    TriangleMesh mesh = createGridMesh(kPatchWorldSize, N);
    m_resourceContext->addGeometry(
        "ocean", createGeometry(*m_renderer, mesh, vertexFormat, VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT));
    m_resourceContext->getGeometry("ocean").setInstanceCount(m_instancesPerSide * m_instancesPerSide);

    auto spectrumImage = createInitialSpectrum();
    m_resourceContext->imageCache.addImageView(
        "randImageView", createView(m_renderer->getDevice(), *spectrumImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1));
    m_resourceContext->imageCache.addImage("randImage", std::move(spectrumImage));

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), 16.0f));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), 16.0f));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), 16.0f, 9.0f));
    imageCache.addImage("brdfLut", integrateBrdfLut(m_renderer));

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 1);
    m_transformHandle = m_transformBuffer->getNextIndex();

    m_envLight = std::make_unique<EnvironmentLight>(
        *m_renderer,
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps/TableMountain").unwrap());

    resetCamera();
}

void OceanScene::resetCamera() {
    constexpr float kAngularSpeed = glm::radians(90.0f);

    const float patchOnScreenSize = kPatchWorldSize * m_modelScale;
    const glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    const float distance = 1.6f * patchOnScreenSize;
    const float yaw = glm::radians(45.0f);
    const float pitch = -glm::atan(1.0f / glm::sqrt(2.0f));

    m_cameraController->setPosition(distance * direction);
    m_cameraController->updateOrientation(yaw / kAngularSpeed, pitch / kAngularSpeed);
    m_cameraController->setSpeed(std::max(0.05f, patchOnScreenSize * 0.15f));
}

OceanScene::~OceanScene() {
    m_passResources.reset();
}

void OceanScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);
    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&OceanOutputData::hdrImage>());
}

void OceanScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& cameraParams = m_cameraController->getCameraParameters();

    m_transformBuffer->getPack(m_transformHandle).M = glm::scale(glm::mat4(1.0f), glm::vec3(m_modelScale));
    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        cameraParams, updateParams.frameInFlightIdx);
    m_transformBuffer->updateStagingBuffer(updateParams.frameInFlightIdx);
    m_skybox->updateTransforms(cameraParams.V, cameraParams.P, updateParams.frameInFlightIdx);

    if (!m_paused) {
        m_oceanParams.time += updateParams.dt;
    }
}

void OceanScene::render(const FrameContext& frameContext) {
    auto* cameraBuffer = m_resourceContext->getRingBuffer("camera");
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead));

    m_renderGraph->execute(frameContext);
}

void OceanScene::drawGui() {
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
    ImGui::SliderFloat("Amplitude", &m_oceanParams.A, 0.0f, 0.01f, "%.5f");
    ImGui::SliderFloat("Small Waves", &m_oceanParams.smallWaves, 0.0f, 4.0f * kCellSize);
    ImGui::SliderFloat("Choppiness", &m_choppiness, 0.0f, 5.0f);
    ImGui::SliderFloat("Model Scale", &m_modelScale, 0.001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
    if (ImGui::SliderInt("Instances Per Side", &m_instancesPerSide, 1, kMaxInstancesPerSide)) {
        m_resourceContext->getGeometry("ocean").setInstanceCount(m_instancesPerSide * m_instancesPerSide);
    }
    if (ImGui::Button("Reset View")) {
        resetCamera();
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    ImGui::End();
}

std::unique_ptr<VulkanImage> OceanScene::createInitialSpectrum() {
    const auto oceanSpectrum{createOceanSpectrum(0, m_oceanParams)};

    auto image = createStorageImage(m_renderer->getDevice(), 1, N, N, VK_FORMAT_R32G32_SFLOAT);
    const auto staging = createStagingBuffer(
        m_renderer->getDevice(), oceanSpectrum.data(), oceanSpectrum.size() * sizeof(oceanSpectrum[0]));
    m_renderer->getDevice().getGeneralQueue().submitAndWait([&staging, img = image.get()](VkCommandBuffer cmdBuffer) {
        const VulkanCommandEncoder encoder(cmdBuffer);
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, kNullStage >> kTransferWrite);
        const VkBufferImageCopy region{
            .bufferRowLength = img->getWidth(),
            .bufferImageHeight = img->getHeight(),
            .imageSubresource =
                {
                    .aspectMask = img->getAspectMask(),
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {img->getWidth(), img->getHeight(), 1},
        };
        encoder.copyBufferToImage(*staging, *img, region);
        encoder.transitionLayout(*img, VK_IMAGE_LAYOUT_GENERAL, kTransferWrite >> kComputeStorageWrite);
    });

    return image;
}

void OceanScene::buildRenderGraph() {
    m_passResources = std::make_unique<OceanPassResources>();
    m_renderGraph = std::make_unique<rg::RenderGraph>();
    m_renderGraph->getBlackboard().insert<OscillationPassData>();
    m_renderGraph->addPass(
        "oscillation",
        [](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
            auto& data = builder.getBlackboard().get<OscillationPassData>();
            data.packedHeightDispX = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-packed-height-dispx", "oscillation"));
            data.packedDispZNormalX = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-packed-dispz-normalx", "oscillation"));
            data.normalZ = builder.createStorageImage(
                {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                fmt::format("{}-normal-z", "oscillation"));
        },
        [this](const FrameContext& ctx) {
            m_passResources->oscillation.bind(ctx);
            ctx.commandEncoder.setPushConstants(
                *m_passResources->oscillation.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);

            ctx.commandEncoder.dispatchCompute(m_passResources->oscillation.dispatchSize);
        });

    auto addFftPasses = [this]<size_t Tag>(const RenderGraphResourceHandle image) {
        struct IfftPushConstants {
            int32_t N;
            int32_t logN;
        };

        const std::string horiPassName{fmt::format("ifft-h-{}", Tag)};
        m_renderGraph->addPass(
            horiPassName,
            [image, horiPassName](rg::RenderGraph::Builder& builder) {
                builder.setType(PassType::Compute);
                builder.readStorageImage(image);
                auto& data = builder.getBlackboard().insert<HorizontalFftPassData<Tag>>();
                data.image = builder.createStorageImage(
                    {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                    fmt::format("{}-image", horiPassName));
            },
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-h-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, IfftPushConstants{N, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });

        const std::string vertPassName{fmt::format("ifft-v-{}", Tag)};
        m_renderGraph->addPass(
            vertPassName,
            [vertPassName](rg::RenderGraph::Builder& builder) {
                builder.setType(PassType::Compute);
                builder.readStorageImage(builder.getBlackboard().get<HorizontalFftPassData<Tag>>().image);
                auto& data = builder.getBlackboard().insert<VerticalFftPassData<Tag>>();
                data.image = builder.createStorageImage(
                    {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                    fmt::format("{}-image", vertPassName));
            },
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-v-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, IfftPushConstants{N, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });
    };
    addFftPasses.operator()<0>(m_renderGraph->getBlackboard().get<OscillationPassData>().packedHeightDispX);
    addFftPasses.operator()<1>(m_renderGraph->getBlackboard().get<OscillationPassData>().packedDispZNormalX);
    addFftPasses.operator()<2>(m_renderGraph->getBlackboard().get<OscillationPassData>().normalZ);

    m_renderGraph->addPass(
        "geometry",
        [this](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<0>>().image);
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image);

            auto& data = builder.getBlackboard().insert<GeometryPassData>();
            auto& geometry = m_resourceContext->getGeometry("ocean");
            data.positions = builder.importBuffer(
                {
                    .formatHint = VK_FORMAT_R32G32B32_SFLOAT,
                    .size = geometry.getVertexBuffer(0)->getSize(),
                    .usageFlags = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT,
                    .externalBuffer = geometry.getVertexBuffer(0)->getHandle(),
                },
                "ocean-positions");
            data.normals = builder.importBuffer(
                {
                    .formatHint = VK_FORMAT_R32G32B32_SFLOAT,
                    .size = geometry.getVertexBuffer(1)->getSize(),
                    .usageFlags = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT,
                    .externalBuffer = geometry.getVertexBuffer(1)->getHandle(),
                },
                "ocean-normals");
        },
        [this](const FrameContext& ctx) {
            struct GeometryUpdateParams {
                int32_t patchSize;
                float patchWorldSize;
                float choppiness;
            };

            m_passResources->geometry.bind(ctx);
            ctx.commandEncoder.setPushConstants(
                *m_passResources->geometry.pipeline->getPipelineLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                GeometryUpdateParams{N, kPatchWorldSize, m_choppiness});
            ctx.commandEncoder.dispatchCompute(m_passResources->geometry.dispatchSize);
        });

    m_renderGraph->addPass(
        kForwardLightingPass,
        [](rg::RenderGraph::Builder& builder) {
            builder.readBuffer(builder.getBlackboard().get<GeometryPassData>().positions, kVertexRead);
            builder.readBuffer(builder.getBlackboard().get<GeometryPassData>().normals, kVertexRead);
            auto& data = builder.getBlackboard().insert<OceanOutputData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kForwardLightingPass),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 0.0f}}});
            builder.exportTexture(data.hdrImage);

            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", kForwardLightingPass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        [this](const FrameContext& ctx) {
            struct OceanVertexPushConstants {
                float patchWorldSize;
                int32_t instancesPerSide;
            };

            auto& geometry = m_resourceContext->getGeometry("ocean");
            ctx.commandEncoder.bindPipeline(*m_oceanPipeline);
            ctx.commandEncoder.setViewport(m_renderer->getDefaultViewport());
            ctx.commandEncoder.setScissor(m_renderer->getDefaultScissor());
            ctx.commandEncoder.setPushConstants(
                *m_oceanPipeline->getPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                OceanVertexPushConstants{kPatchWorldSize, m_instancesPerSide});
            ctx.commandEncoder.bindDescriptorSets(m_oceanMaterial->getDescriptorSetBinding());
            geometry.bindAndDraw(ctx.commandEncoder);

            const RenderNode& skyboxNode = m_skybox->getRenderNode();
            const auto& skyboxMaterialData = skyboxNode.materials.at({kForwardLightingPass, 0}).at(-1);
            ctx.commandEncoder.bindPipeline(*skyboxMaterialData.material->getPipeline());
            ctx.commandEncoder.bindDescriptorSets(skyboxMaterialData.material->getDescriptorSetBinding());
            skyboxNode.geometry->bindAndDraw(ctx.commandEncoder);
        });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&OceanOutputData::hdrImage>());

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_envLight->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));

    m_passResources->oscillation =
        createOscillationPassDispatch(*m_renderer, *m_renderGraph, m_resourceContext->imageCache);
    createFftDispatches<0>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(
            m_renderGraph->getBlackboard().get<OscillationPassData>().packedHeightDispX));
    createFftDispatches<1>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(
            m_renderGraph->getBlackboard().get<OscillationPassData>().packedDispZNormalX));
    createFftDispatches<2>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().normalZ));

    const auto finalFftView = [this]<size_t Tag>() -> const VulkanImageView& {
        return m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<VerticalFftPassData<Tag>>().image);
    };

    auto& geometryDispatch = m_passResources->geometry;
    geometryDispatch.workGroupSize = {16, 16, 1};
    geometryDispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N + 1, N + 1, 1), geometryDispatch.workGroupSize);
    geometryDispatch.pipeline =
        createComputePipeline(*m_renderer, "ocean-geometry.comp", geometryDispatch.workGroupSize);
    geometryDispatch.material = std::make_unique<Material>(geometryDispatch.pipeline.get());
    auto& geometry = m_resourceContext->getGeometry("ocean");
    geometryDispatch.material->writeDescriptor(0, 0, geometry.getVertexBuffer(0)->createDescriptorInfo());
    geometryDispatch.material->writeDescriptor(0, 1, geometry.getVertexBuffer(1)->createDescriptorInfo());
    auto& linearRepeat = m_resourceContext->imageCache.getSampler("linearRepeat");
    geometryDispatch.material->writeDescriptor(0, 2, finalFftView.operator()<0>(), linearRepeat);
    geometryDispatch.material->writeDescriptor(0, 3, finalFftView.operator()<1>(), linearRepeat);

    m_oceanPipeline = m_resourceContext->createPipeline(
        "ocean", "Ocean.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    m_oceanMaterial = m_resourceContext->createMaterial("ocean", m_oceanPipeline);
    m_oceanMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    m_oceanMaterial->writeDescriptor(0, 1, finalFftView.operator()<0>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 2, finalFftView.operator()<1>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 3, finalFftView.operator()<2>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(1, 0, *m_resourceContext->getRingBuffer("camera"));
    m_oceanMaterial->writeDescriptor(
        1, 1, m_envLight->getDiffuseMapView(), m_resourceContext->imageCache.getSampler("linearClamp"));
    m_oceanMaterial->writeDescriptor(
        1, 2, m_envLight->getSpecularMapView(), m_resourceContext->imageCache.getSampler("linearMipmap"));
    m_oceanMaterial->writeDescriptor(
        1,
        3,
        m_resourceContext->imageCache.getImage("brdfLut").getView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();
}

} // namespace crisp
