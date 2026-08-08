
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
    FlatStringHashMap<ComputeDispatch> bitReverse;
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
    Renderer& renderer, const VulkanImageView& sourceView, const VulkanImageView& dstView) {
    ComputeDispatch dispatch{};
    dispatch.workGroupSize = {16, 16, 1};
    dispatch.dispatchSize = computeWorkGroupCount(glm::uvec3(N, N, 1), dispatch.workGroupSize);
    dispatch.pipeline = createComputePipeline(renderer, "ocean-reverse-bits.comp", dispatch.workGroupSize);
    dispatch.material = std::make_unique<Material>(dispatch.pipeline.get());
    dispatch.material->writeDescriptor(0, 0, sourceView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    dispatch.material->writeDescriptor(0, 1, dstView.getDescriptorInfo(nullptr, VK_IMAGE_LAYOUT_GENERAL));
    return dispatch;
}

template <size_t Tag, bool Horizontal>
ComputeDispatch createIfftDispatch(
    Renderer& renderer, const VulkanImageView& sourceView, const VulkanImageView& dstView) {
    constexpr glm::uvec3 workAmount = Horizontal ? glm::uvec3(N / 2, N, 1) : glm::uvec3(N, N / 2, 1);
    const std::string shaderName = Horizontal ? "ifft-hori.comp" : "ifft-vert.comp";

    ComputeDispatch dispatch{};
    dispatch.workGroupSize = {16, 16, 1};
    dispatch.dispatchSize = computeWorkGroupCount(workAmount, dispatch.workGroupSize);
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
    passResources.bitReverse[fmt::format("bit-reverse-h-{}", Tag)] = createBitReverseDispatch<Tag>(
        renderer,
        srcView,
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image));
    for (int32_t i = 0; i < logN; ++i) {
        if (i == 0) {
            passResources.ifft[fmt::format("ifft-h-{}-{}", Tag, i)] = createIfftDispatch<Tag, true>(
                renderer,
                renderGraph.getResourceImageView(
                    renderGraph.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image),
                renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i]));
        } else {
            passResources.ifft[fmt::format("ifft-h-{}-{}", Tag, i)] = createIfftDispatch<Tag, true>(
                renderer,
                renderGraph.getResourceImageView(
                    renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i - 1]),
                renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image[i]));
        }
    }

    passResources.bitReverse[fmt::format("bit-reverse-v-{}", Tag)] = createBitReverseDispatch<Tag>(
        renderer,
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<HorizontalFftPassData<Tag>>().image.back()),
        renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalBitReversePassData<Tag>>().image));
    for (int32_t i = 0; i < logN; ++i) {
        if (i == 0) {
            passResources.ifft[fmt::format("ifft-v-{}-{}", Tag, i)] = createIfftDispatch<Tag, false>(
                renderer,
                renderGraph.getResourceImageView(
                    renderGraph.getBlackboard().get<VerticalBitReversePassData<Tag>>().image),
                renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i]));
        } else {
            passResources.ifft[fmt::format("ifft-v-{}-{}", Tag, i)] = createIfftDispatch<Tag, false>(
                renderer,
                renderGraph.getResourceImageView(
                    renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i - 1]),
                renderGraph.getResourceImageView(renderGraph.getBlackboard().get<VerticalFftPassData<Tag>>().image[i]));
        }
    }
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

    // Set M before update(), which derives MV, MVP and the normal matrix from it.
    m_transformBuffer->getPack(m_transformHandle).M = glm::scale(glm::mat4(1.0f), glm::vec3(m_modelScale));
    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        cameraParams, updateParams.frameInFlightIdx);
    m_transformBuffer->updateStagingBuffer(updateParams.frameInFlightIdx);

    if (!m_paused) {
        m_oceanParams.time += updateParams.dt;
    }
}

void OceanScene::render(const FrameContext& frameContext) {
    auto* cameraBuffer = m_resourceContext->getRingBuffer("camera");
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
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
        [this](const FrameContext& ctx) {
            m_passResources->oscillation.bind(ctx);
            ctx.commandEncoder.setPushConstants(
                *m_passResources->oscillation.pipeline->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, m_oceanParams);

            ctx.commandEncoder.dispatchCompute(m_passResources->oscillation.dispatchSize);
        });

    auto addFftPasses = [this]<size_t Tag>(const RenderGraphResourceHandle image) {
        struct BitReversalPushConstants {
            int32_t reversalDirection; // 0 is horizontal, 1 is vertical.
            int32_t passCount;         // Logarithm of the patch discretization size.
        };

        const std::string bitReversePassHorName{fmt::format("bit-reverse-h-{}", Tag)};
        m_renderGraph->addPass(
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
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->bitReverse.at(fmt::format("bit-reverse-h-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    BitReversalPushConstants{0, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });

        struct IFFTPushConstants {
            int32_t passIdx; // Index of the current pass.
            int32_t N;       // Number of elements in the array.
        };
        for (int i = 0; i < logN; ++i) {

            const std::string passName = fmt::format("ifft-h-{}-{}", Tag, i);
            m_renderGraph->addPass(
                passName,
                [passName, i](rg::RenderGraph::Builder& builder) {
                    builder.setType(PassType::Compute);
                    auto& data =
                        i == 0 ? builder.getBlackboard().insert<HorizontalFftPassData<Tag>>()
                               : builder.getBlackboard().get<HorizontalFftPassData<Tag>>();
                    if (i == 0) {
                        data.image.resize(logN);
                        builder.readStorageImage(builder.getBlackboard().get<HorizontalBitReversePassData<Tag>>().image);
                    } else {
                        builder.readStorageImage(data.image[i - 1]);
                    }
                    data.image[i] = builder.createStorageImage(
                        {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                        fmt::format("{}-image", passName));
                },
                [this, i](const FrameContext& ctx) {
                    const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-h-{}-{}", Tag, i))};
                    dispatch.bind(ctx);
                    ctx.commandEncoder.setPushConstants(
                        *dispatch.pipeline->getPipelineLayout(),
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        IFFTPushConstants{i + 1, N});
                    ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
                });
        }

        const std::string bitReversePassVertName{fmt::format("bit-reverse-v-{}", Tag)};
        m_renderGraph->addPass(
            bitReversePassVertName,
            [bitReversePassVertName](rg::RenderGraph::Builder& builder) {
                builder.setType(PassType::Compute);
                builder.readStorageImage(builder.getBlackboard().get<HorizontalFftPassData<Tag>>().image.back());
                auto& data = builder.getBlackboard().insert<VerticalBitReversePassData<Tag>>();
                data.image = builder.createStorageImage(
                    {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                    fmt::format("{}-image", bitReversePassVertName));
            },
            [this](const FrameContext& ctx) {
                const auto& dispatch{m_passResources->bitReverse.at(fmt::format("bit-reverse-v-{}", Tag))};
                dispatch.bind(ctx);
                ctx.commandEncoder.setPushConstants(
                    *dispatch.pipeline->getPipelineLayout(),
                    VK_SHADER_STAGE_COMPUTE_BIT,
                    BitReversalPushConstants{1, logN});
                ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
            });

        for (int i = 0; i < logN; ++i) {
            const std::string passName = fmt::format("ifft-v-{}-{}", Tag, i);
            m_renderGraph->addPass(
                passName,
                [passName, i](rg::RenderGraph::Builder& builder) {
                    builder.setType(PassType::Compute);
                    auto& data =
                        i == 0 ? builder.getBlackboard().insert<VerticalFftPassData<Tag>>()
                               : builder.getBlackboard().get<VerticalFftPassData<Tag>>();
                    if (i == 0) {
                        data.image.resize(logN);
                        builder.readStorageImage(builder.getBlackboard().get<VerticalBitReversePassData<Tag>>().image);
                    } else {
                        builder.readStorageImage(data.image[i - 1]);
                    }
                    data.image[i] = builder.createStorageImage(
                        {.sizePolicy = SizePolicy::Absolute, .width = N, .height = N, .format = VK_FORMAT_R32G32_SFLOAT},
                        fmt::format("{}-image", passName));
                },
                [this, i](const FrameContext& ctx) {
                    const auto& dispatch{m_passResources->ifft.at(fmt::format("ifft-v-{}-{}", Tag, i))};
                    dispatch.bind(ctx);
                    ctx.commandEncoder.setPushConstants(
                        *dispatch.pipeline->getPipelineLayout(),
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        IFFTPushConstants{i + 1, N});
                    ctx.commandEncoder.dispatchCompute(dispatch.dispatchSize);
                });
        }
    };
    addFftPasses.operator()<0>(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementY);
    addFftPasses.operator()<1>(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementX);
    addFftPasses.operator()<2>(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementZ);
    addFftPasses.operator()<3>(m_renderGraph->getBlackboard().get<OscillationPassData>().normalX);
    addFftPasses.operator()<4>(m_renderGraph->getBlackboard().get<OscillationPassData>().normalZ);

    m_renderGraph->addPass(
        "geometry",
        [this](rg::RenderGraph::Builder& builder) {
            builder.setType(PassType::Compute);
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<0>>().image.back());
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<1>>().image.back());
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<2>>().image.back());
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<3>>().image.back());
            builder.readTexture(builder.getBlackboard().get<VerticalFftPassData<4>>().image.back());

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
        });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&OceanOutputData::hdrImage>());

    m_passResources->oscillation =
        createOscillationPassDispatch(*m_renderer, *m_renderGraph, m_resourceContext->imageCache);
    createFftDispatches<0>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementY));
    createFftDispatches<1>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementX));
    createFftDispatches<2>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().displacementZ));
    createFftDispatches<3>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().normalX));
    createFftDispatches<4>(
        *m_passResources,
        *m_renderer,
        *m_renderGraph,
        m_renderGraph->getResourceImageView(m_renderGraph->getBlackboard().get<OscillationPassData>().normalZ));

    const auto finalFftView = [this]<size_t Tag>() -> const VulkanImageView& {
        return m_renderGraph->getResourceImageView(
            m_renderGraph->getBlackboard().get<VerticalFftPassData<Tag>>().image.back());
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
    geometryDispatch.material->writeDescriptor(0, 4, finalFftView.operator()<2>(), linearRepeat);
    geometryDispatch.material->writeDescriptor(0, 5, finalFftView.operator()<3>(), linearRepeat);
    geometryDispatch.material->writeDescriptor(0, 6, finalFftView.operator()<4>(), linearRepeat);

    m_oceanPipeline = m_resourceContext->createPipeline(
        "ocean", "Ocean.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    m_oceanMaterial = m_resourceContext->createMaterial("ocean", m_oceanPipeline);
    m_oceanMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    m_oceanMaterial->writeDescriptor(0, 1, finalFftView.operator()<0>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 2, finalFftView.operator()<1>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 3, finalFftView.operator()<2>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 4, finalFftView.operator()<3>(), linearRepeat);
    m_oceanMaterial->writeDescriptor(0, 5, finalFftView.operator()<4>(), linearRepeat);
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
