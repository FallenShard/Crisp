#include <Crisp/Scenes/AmbientOcclusionScene.hpp>

#include <random>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Math/Warp.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
namespace {

struct AmbientOcclusionData {
    RenderGraphResourceHandle image;
};

struct BlurHorizontalPassData {
    RenderGraphResourceHandle image;
};

struct BlurVerticalPassData {
    RenderGraphResourceHandle image;
};

constexpr glm::vec4 kColorPushConstant(0.2f, 0.0f, 0.0f, 1.0f);

struct BlurParams {
    float dirX;
    float dirY;
    float sigma;
    int radius;
};

constexpr BlurParams kBlurH = {0.0f, 1.0f / Application::kDefaultWindowWidth, 1.0f, 3};
constexpr BlurParams kBlurV = {1.0f / Application::kDefaultWindowHeight, 0.0f, 1.0f, 3};

void createDrawCommand(
    std::vector<DrawCommand>& drawCommands,
    const RenderNode& renderNode,
    const std::string_view renderPass,
    const uint32_t virtualFrameIndex) {
    if (!renderNode.isVisible) {
        return;
    }

    for (const auto& [key, materialMap] : renderNode.materials) {
        for (const auto& [part, material] : materialMap) {
            if (key.renderPassName == renderPass) {
                drawCommands.push_back(material.createDrawCommand(virtualFrameIndex, renderNode));
            }
        }
    }
}

void drawPostProcessEffect(
    Renderer& renderer, const PostProcessingDrawCommand& command, const RenderPassExecutionContext& ctx) {
    command.pipeline->bind(ctx.cmdBuffer.getHandle());
    const auto dynamicState = command.pipeline->getDynamicStateFlags();
    if (dynamicState & PipelineDynamicState::Viewport) {
        renderer.setDefaultViewport(ctx.cmdBuffer.getHandle());
    }

    if (dynamicState & PipelineDynamicState::Scissor) {
        renderer.setDefaultScissor(ctx.cmdBuffer.getHandle());
    }

    command.pipeline->getPipelineLayout()->setPushConstants(
        ctx.cmdBuffer.getHandle(), static_cast<const char*>(command.pushConstantView.data));

    if (command.material) {
        const auto& dynamicBufferViews = command.material->getDynamicBufferViews();
        std::vector<uint32_t> dynamicBufferOffsets(dynamicBufferViews.size());
        for (std::size_t i = 0; i < dynamicBufferOffsets.size(); ++i) {
            dynamicBufferOffsets[i] =
                dynamicBufferViews[i].buffer->getDynamicOffset(ctx.virtualFrameIndex) + dynamicBufferViews[i].subOffset;
        }
        command.material->bind(ctx.virtualFrameIndex, ctx.cmdBuffer.getHandle(), dynamicBufferOffsets);
    }

    renderer.getFullScreenGeometry()->bindAndDraw(ctx.cmdBuffer.getHandle());
}

std::unique_ptr<VulkanImage> createRandomRotationImage(Renderer& renderer) {
    std::default_random_engine rng(43);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<glm::vec4> noiseTexData;
    noiseTexData.reserve(16);
    for (int i = 0; i < 16; i++) {
        noiseTexData.emplace_back(
            glm::normalize(glm::vec3{dist(rng) * 2.0f - 1.0f, dist(rng) * 2.0f - 1.0f, 0.0f}), 1.0f);
    }

    VkImageCreateInfo noiseTexInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    noiseTexInfo.flags = 0;
    noiseTexInfo.imageType = VK_IMAGE_TYPE_2D;
    noiseTexInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    noiseTexInfo.extent = {4, 4, 1u};
    noiseTexInfo.mipLevels = 1;
    noiseTexInfo.arrayLayers = 1;
    noiseTexInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    noiseTexInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    noiseTexInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    noiseTexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    noiseTexInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return createVulkanImage(renderer, noiseTexData.size() * sizeof(glm::vec4), noiseTexData.data(), noiseTexInfo);
}

std::unique_ptr<UniformBuffer> createHemisphereSampleBuffer(Renderer* renderer) {
    std::default_random_engine randomEngine(42);
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    std::array<glm::vec4, 512> samples; // NOLINT
    for (auto& sample : samples) {
        float x = distribution(randomEngine);
        float y = distribution(randomEngine);
        float r = distribution(randomEngine);
        sample = glm::vec4(warp::cubeToUniformHemisphereVolume({x, y, r}), 1.0f);
    }

    return std::make_unique<UniformBuffer>(renderer, sizeof(samples), BufferUpdatePolicy::Constant, samples.data());
}

} // namespace

AmbientOcclusionScene::AmbientOcclusionScene(Renderer* renderer, Window* window)
    : Scene(renderer, window)
    , m_ssaoParams{128, 0.5f} {
    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_cameraController->setPosition(glm::vec3(0.0f, 2.0f, -1.0f));
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addImageWithView("noise", createRandomRotationImage(*m_renderer));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice()));
    imageCache.addSampler("nearestClamp", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice()));

    m_resourceContext->addUniformBuffer("samples", createHemisphereSampleBuffer(renderer));

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 2);

    m_rg = std::make_unique<rg::RenderGraph>();
    m_rg->addPass(
        kForwardLightingPass,
        [](rg::RenderGraph::Builder& builder) {
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
        [this](const RenderPassExecutionContext& ctx) {
            std::vector<DrawCommand> drawCommands{};
            createDrawCommand(drawCommands, *m_floorNode, kForwardLightingPass, ctx.virtualFrameIndex);
            createDrawCommand(drawCommands, *m_sponzaNode, kForwardLightingPass, ctx.virtualFrameIndex);

            for (const auto& drawCommand : drawCommands) {
                RenderGraph::executeDrawCommand(drawCommand, *m_renderer, ctx.cmdBuffer, ctx.virtualFrameIndex);
            }
        });

    m_rg->addPass(
        "ssao",
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<ForwardLightingData>().hdrImage);
            auto& data = builder.getBlackboard().insert<AmbientOcclusionData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-image", "ssao"));
        },
        [this](const RenderPassExecutionContext& ctx) {
            drawPostProcessEffect(*m_renderer, m_postProcessingCommands["ssao"], ctx);
        });

    m_rg->addPass(
        "blur-h",
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<AmbientOcclusionData>().image);
            auto& data = builder.getBlackboard().insert<BlurHorizontalPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-image", "blur-h"));
        },
        [this](const RenderPassExecutionContext& ctx) {
            drawPostProcessEffect(*m_renderer, m_postProcessingCommands["blur-h"], ctx);
        });

    m_rg->addPass(
        "blur-v",
        [](rg::RenderGraph::Builder& builder) {
            builder.readTexture(builder.getBlackboard().get<BlurHorizontalPassData>().image);

            auto& data = builder.getBlackboard().insert<BlurVerticalPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-image", "blur-v"),
                VkClearValue{.color = {{1.0f, 1.0f, 0.0f, 0.0f}}});

            builder.exportTexture(data.image);
        },
        [this](const RenderPassExecutionContext& ctx) {
            drawPostProcessEffect(*m_renderer, m_postProcessingCommands["blur-v"], ctx);
        });

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
        const auto& data = m_rg->getBlackboard().get<BlurVerticalPassData>();
        m_sceneImageViews.resize(kRendererVirtualFrameCount);
        for (auto& sv : m_sceneImageViews) {
            sv = m_rg->createViewFromResource(m_renderer->getDevice(), data.image);
        }

        m_renderer->setSceneImageViews(m_sceneImageViews);
    });
    m_renderer->flushResourceUpdates(true);

    auto& ssao = m_postProcessingCommands["ssao"];
    ssao.pipeline = m_resourceContext->createPipeline("ssao", "Ssao.json", m_rg->getRenderPass("ssao"), 0);
    ssao.material = m_resourceContext->createMaterial("ssao", ssao.pipeline);
    ssao.material->writeDescriptor(
        0, 0, m_rg->getRenderPass(kForwardLightingPass), 0, &imageCache.getSampler("nearestClamp"));
    ssao.material->writeDescriptor(0, 1, *m_resourceContext->getUniformBuffer("camera"));
    ssao.material->writeDescriptor(0, 2, *m_resourceContext->getUniformBuffer("samples"));
    ssao.material->writeDescriptor(0, 3, imageCache.getImageView("noise"), imageCache.getSampler("linearRepeat"));
    ssao.pushConstantView.set(m_ssaoParams);

    auto& blurH = m_postProcessingCommands["blur-h"];
    blurH.pipeline = m_resourceContext->createPipeline("blur-h", "GaussianBlur.json", m_rg->getRenderPass("blur-h"), 0);
    blurH.material = m_resourceContext->createMaterial("blur-h", blurH.pipeline);
    blurH.material->writeDescriptor(0, 0, m_rg->getRenderPass("ssao"), 0, &imageCache.getSampler("linearClamp"));
    blurH.pushConstantView.set(kBlurH);

    auto& blurV = m_postProcessingCommands["blur-v"];
    blurV.pipeline = m_resourceContext->createPipeline("blur-v", "GaussianBlur.json", m_rg->getRenderPass("blur-v"), 0);
    blurV.material = m_resourceContext->createMaterial("blur-v", blurV.pipeline);
    blurV.material->writeDescriptor(0, 0, m_rg->getRenderPass("blur-h"), 0, &imageCache.getSampler("linearClamp"));
    blurV.pushConstantView.set(kBlurV);

    VulkanPipeline* colorPipeline =
        m_resourceContext->createPipeline("color", "UniformColor.json", m_rg->getRenderPass(kForwardLightingPass), 0);
    Material* colorMaterial = m_resourceContext->createMaterial("color", colorPipeline);
    colorMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    VulkanPipeline* normalPipeline =
        m_resourceContext->createPipeline("normal", "DepthNormal.json", m_rg->getRenderPass(kForwardLightingPass), 0);
    Material* normalMaterial = m_resourceContext->createMaterial("normal", normalPipeline);
    normalMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    m_renderer->getDevice().flushDescriptorUpdates();

    m_resourceContext->addGeometry(
        "floorPos", createFromMesh(*m_renderer, createPlaneMesh(flatten(kPosVertexFormat)), kPosVertexFormat));

    const VertexLayoutDescription vertexFormat = {{VertexAttribute::Position, VertexAttribute::Normal}};
    m_resourceContext->addGeometry(
        "sponza",
        createFromMesh(
            *m_renderer,
            loadTriangleMesh(m_renderer->getResourcesPath() / "Meshes/Sponza-master/sponza.obj", flatten(vertexFormat))
                .unwrap(),
            vertexFormat));

    // m_skybox = std::make_unique<Skybox>(m_renderer, *m_rg->getRenderPass(kForwardLightingPass), "Creek");

    m_floorNode = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    m_floorNode->transformPack->M =
        glm::translate(glm::vec3(0.0f, -1.0f, 0.0f)) * glm::scale(glm::vec3(50.0f, 1.0f, 50.0f));
    m_floorNode->geometry = m_resourceContext->getGeometry("floorPos");
    m_floorNode->pass(kForwardLightingPass).material = colorMaterial;
    m_floorNode->pass(kForwardLightingPass).pipeline = colorPipeline;
    m_floorNode->pass(kForwardLightingPass).setPushConstantView(kColorPushConstant);

    m_sponzaNode = std::make_unique<RenderNode>(*m_transformBuffer, m_transformBuffer->getNextIndex());
    m_sponzaNode->transformPack->M = glm::scale(glm::vec3(0.01f));
    m_sponzaNode->geometry = m_resourceContext->getGeometry("sponza");
    m_sponzaNode->pass(kForwardLightingPass).material = normalMaterial;
    m_sponzaNode->pass(kForwardLightingPass).pipeline = normalPipeline;

    // // mainPassNode.renderNodes.push_back(m_skybox->createRenderNode());
}

void AmbientOcclusionScene::resize(int width, int height) {
    // m_cameraController->onViewportResized(width, height);

    // m_renderGraph->resize(width, height);
    // m_renderer->setSceneImageView(m_renderGraph->getNode("blurVPass").renderPass.get(), 0);

    // auto* mainPass = m_renderGraph->getNode("mainPass").renderPass.get();
    // m_resourceContext->getMaterial("Ssao.lua")
    //     ->writeDescriptor(0, 0, *mainPass, 0, &m_resourceContext->imageCache.getSampler("nearestClamp"));
}

void AmbientOcclusionScene::update(float dt) {
    m_cameraController->update(dt);
    const CameraParameters cameraParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(cameraParams);
    // auto pos = m_cameraController->getCamera().getPosition();

    m_transformBuffer->update(cameraParams.V, cameraParams.P);
    // m_skybox->updateTransforms(cameraParams.V, cameraParams.P);
}

void AmbientOcclusionScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        m_rg->execute(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex());
    });
}

void AmbientOcclusionScene::renderGui() {
    ImGui::Begin("Ambient Occlusion");
    ImGui::SliderInt("Sample Count", &m_ssaoParams.sampleCount, 1, 512);
    ImGui::SliderFloat("Radius", &m_ssaoParams.radius, 0.1f, 2.0f);
    ImGui::End();

    drawCameraUi(m_cameraController->getCamera());
}

} // namespace crisp