#include <Crisp/Scenes/ClusteredLightingScene.hpp>

#include <imgui.h>

#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Lights/LightCullingPass.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Vulkan/Rhi/VulkanSampler.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("ClusteredLightingScene");

constexpr const char* kCameraBufferId = "camera";
constexpr const char* kMaterialBufferId = "clusteredMaterialParams";
constexpr const char* kMaterialId = "clusteredLighting";
constexpr const char* kDepthMaterialId = "clusteredDepthPrepass";
constexpr const char* kDepthPrepass = "clusteredDepthPrepass";

constexpr float kFloorExtent = 200.0f;
constexpr uint32_t kMaximumObjectCount = 512;

constexpr int32_t kSphereGridX = 12;
constexpr int32_t kSphereGridZ = 8;
constexpr float kSphereSpacing = 12.0f;

struct DrawCommandRecordingState {
    const VulkanPipeline* pipeline{nullptr};
};

void executeDrawCommand(
    const DrawCommand& command, const VulkanCommandEncoder& encoder, DrawCommandRecordingState& state) {
    if (state.pipeline != command.pipeline) {
        encoder.bindPipeline(*command.pipeline);
        state.pipeline = command.pipeline;
    }
    if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Viewport) &&
        command.viewport.width != 0.0f) {
        encoder.setViewport(command.viewport);
    }
    if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Scissor) &&
        command.scissor.extent.width != 0) {
        encoder.setScissor(command.scissor);
    }

    encoder.setPushConstants(*command.pipeline->getPipelineLayout(), command.pushConstantView.asSpan());
    if (command.material) {
        encoder.bindDescriptorSets(command.material->getDescriptorSetBinding(command.getDynamicBufferOffsets()));
    }
    command.geometry->bindVertexBuffers(encoder, command.firstBuffer, command.bufferCount);
    command.drawFunc(encoder, command.geometryView);
}

void drawNode(
    const RenderNode& node,
    const std::string_view renderPass,
    const VulkanCommandEncoder& encoder,
    DrawCommandRecordingState& state) {
    for (const auto& [key, materialMap] : node.materials) {
        if (key.renderPassName != renderPass) {
            continue;
        }
        for (const auto& [part, material] : materialMap) {
            executeDrawCommand(material.createDrawCommand(node), encoder, state);
        }
    }
}
} // namespace

ClusteredLightingScene::ClusteredLightingScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_cameraController->setPosition({0.0f, 12.0f, 60.0f});
    m_cameraController->setSpeed(30.0f);
    m_resourceContext->createUniformRingBuffer(kCameraBufferId, sizeof(CameraParameters));
    m_resourceContext->createRingBufferFromStruct(
        kMaterialBufferId, m_materialParams, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT);

    m_pointLightCount = args.value("pointLightCount", m_pointLightCount);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        /*shadowMapSize=*/1,
        /*cascadeCount=*/0);
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps/GreenwichPark").unwrap(),
        "GreenwichPark");
    regeneratePointLights();
    recreateLightClustering();

    m_renderGraph = std::make_unique<rg::RenderGraph>();

    // Lays down depth so the culling pass can bound each tile by the surfaces actually visible in it,
    // and so the forward pass gets early-Z for free.
    RenderGraphResourceHandle depthImage{};
    m_renderGraph->addPass(
        kDepthPrepass,
        PassType::Rasterizer,
        [&depthImage](rg::RenderGraph::Builder& builder) {
            depthImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", kDepthPrepass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        [this](const FrameContext& ctx) {
            DrawCommandRecordingState recordingState{};
            for (const auto& [id, node] : m_renderNodes) {
                drawNode(*node, kDepthPrepass, ctx.commandEncoder, recordingState);
            }
        });

    addLightCullingPass(*m_renderGraph, *m_renderer, *m_resourceContext, *m_lightSystem, kCameraBufferId);

    m_renderGraph->addPass(
        kForwardLightingPass,
        PassType::Rasterizer,
        [depthImage](rg::RenderGraph::Builder& builder) {
            auto& data = builder.getBlackboard().insert<ForwardLightingPassData>();
            data.hdrImage = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kForwardLightingPass),
                VkClearValue{.color{{0.0f, 0.0f, 0.0f, 1.0f}}});
            builder.exportTexture(data.hdrImage);
            builder.readWriteAttachment(depthImage);
        },
        [this](const FrameContext& ctx) {
            DrawCommandRecordingState recordingState{};
            for (const auto& [id, node] : m_renderNodes) {
                drawNode(*node, kForwardLightingPass, ctx.commandEncoder, recordingState);
            }
            drawNode(m_skybox->getRenderNode(), kForwardLightingPass, ctx.commandEncoder, recordingState);
        });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaximumObjectCount);

    createCommonTextures();
    createSceneObjects();

    CRISP_LOGI("Clustered lighting scene ready with {} point lights.", m_pointLightCount);
}

void ClusteredLightingScene::resize(const int width, const int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    recreateLightClustering();
}

void ClusteredLightingScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    m_transformBuffer->update(camParams.V, camParams.P);
}

void ClusteredLightingScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("ClusteredLightingScene::render", frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        (kVertexUniformRead | kFragmentUniformRead | kFragmentRead | kComputeRead) >> kTransferWrite);

    const auto camParams = m_cameraController->getCameraParameters();
    m_lightSystem->update(m_cameraController->getCamera(), frameContext.virtualFrameIndex);
    m_lightSystem->getPointLightBuffer()->updateDeviceBuffer(frameContext.commandEncoder);

    m_skybox->updateTransforms(camParams.V, camParams.P, frameContext.virtualFrameIndex);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);

    m_materialParams.debugMode = m_showClusterHeatmap ? 1 : 0;
    auto* materialBuffer = m_resourceContext->getRingBuffer(kMaterialBufferId);
    materialBuffer->updateStagingBufferFromStruct(m_materialParams, frameContext.virtualFrameIndex);
    materialBuffer->updateDeviceBuffer(frameContext.commandEncoder);

    auto* cameraBuffer = m_resourceContext->getRingBuffer(kCameraBufferId);
    cameraBuffer->updateStagingBufferFromStruct(camParams, frameContext.virtualFrameIndex);
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->updateStagingBuffer(frameContext.virtualFrameIndex);
    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead | kFragmentRead | kComputeRead));

    m_renderGraph->execute(frameContext);
}

void ClusteredLightingScene::drawGui() {
    ImGui::Begin("Scene");
    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraUi(m_cameraController->getCamera(), /*isSeparateWindow=*/false);
    }
    if (ImGui::CollapsingHeader("Material")) {
        ImGui::ColorEdit3("Albedo", &m_materialParams.albedo.x);
        ImGui::SliderFloat("Metallic", &m_materialParams.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m_materialParams.roughness, 0.02f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Light Clustering")) {
        const auto& clustering = m_lightSystem->getLightClustering();
        const glm::ivec3 grid{clustering.m_clusterGridSize};
        ImGui::Text("Cluster tile: %d px, %d depth slices", kClusterTileSize, kClusterDepthSliceCount);
        ImGui::Text("Cluster grid: %d x %d x %d (%u clusters)", grid.x, grid.y, grid.z, clustering.getClusterCount());
        ImGui::Text("Max lights per cluster: %u", kMaxLightsPerCluster);
        ImGui::Checkbox("Cluster Light Count Heatmap", &m_showClusterHeatmap);

        ImGui::SliderInt("Point Lights", &m_pointLightCount, 1, 4096);
        if (ImGui::Button("Regenerate Lights")) {
            m_renderer->finish();
            regeneratePointLights();
            recreateLightClustering();
        }
    }
    ImGui::End();

    ImGui::Begin("Render Graph");
    drawRenderGraphGui(*m_renderGraph);
    ImGui::End();
}

void ClusteredLightingScene::regeneratePointLights() {
    m_lightSystem->createPointLightBuffer(createRandomPointLights(static_cast<uint32_t>(m_pointLightCount)));
}

void ClusteredLightingScene::recreateLightClustering() {
    m_lightSystem->createTileGridBuffers(m_cameraController->getCameraParameters());

    if (m_material != nullptr) {
        m_material->writeDescriptor(1, 0, *m_lightSystem->getPointLightBuffer());
        m_material->writeDescriptor(1, 1, *m_lightSystem->getLightIndexBuffer());
        m_material->writeDescriptor(3, 0, *m_lightSystem->getLightGridBuffer());
        m_renderer->getDevice().flushDescriptorUpdates();
    }
}

RenderNode& ClusteredLightingScene::createRenderNode(const std::string_view id) {
    const auto transformHandle{m_transformBuffer->getNextIndex()};
    return *m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle)).first->second;
}

void ClusteredLightingScene::createCommonTextures() {
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addImage("brdfLut", loadBrdfLut(m_renderer));

    auto* depthPipeline = m_resourceContext->createPipeline(
        kDepthMaterialId, "ClusteredDepthPrepass.json", {m_renderGraph->getRasterizationPassDescriptor(kDepthPrepass)});
    m_depthMaterial = m_resourceContext->createMaterial(kDepthMaterialId, depthPipeline);
    m_depthMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());

    auto* pipeline = m_resourceContext->createPipeline(
        kMaterialId, "ClusteredLighting.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    m_material = m_resourceContext->createMaterial(kMaterialId, pipeline);

    m_material->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
    m_material->writeDescriptor(0, 1, *m_resourceContext->getRingBuffer(kCameraBufferId));
    m_material->writeDescriptor(0, 2, *m_resourceContext->getRingBuffer(kMaterialBufferId));

    m_material->writeDescriptor(1, 0, *m_lightSystem->getPointLightBuffer());
    m_material->writeDescriptor(1, 1, *m_lightSystem->getLightIndexBuffer());

    const auto& envLight = *m_lightSystem->getEnvironmentLight();
    m_material->writeDescriptor(2, 0, envLight.getDiffuseIrradianceShBuffer());
    m_material->writeDescriptor(2, 1, envLight.getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    m_material->writeDescriptor(2, 2, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));

    m_material->writeDescriptor(3, 0, *m_lightSystem->getLightGridBuffer());

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        envLight.getCubeMapView(),
        imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();
}

void ClusteredLightingScene::addDepthPrepassEntry(RenderNode& node, Geometry& geometry) {
    auto& depthPass = node.pass(kDepthPrepass);
    depthPass.setGeometry(&geometry, 0, 1);
    depthPass.material = m_depthMaterial;
}

void ClusteredLightingScene::createSceneObjects() {
    auto& floorGeometry = m_resourceContext->addGeometry(
        "floor", createGeometry(*m_renderer, createPlaneMesh(kFloorExtent, kFloorExtent), kPbrVertexFormat));
    auto& floor = createRenderNode("floor");
    floor.geometry = &floorGeometry;
    floor.transformPack->M = glm::mat4(1.0f);
    floor.pass(kForwardLightingPass).material = m_material;
    addDepthPrepassEntry(floor, floorGeometry);

    auto& sphereGeometry =
        m_resourceContext->addGeometry("sphere", createGeometry(*m_renderer, createSphereMesh(), kPbrVertexFormat));
    for (int32_t i = 0; i < kSphereGridX; ++i) {
        for (int32_t j = 0; j < kSphereGridZ; ++j) {
            const float x = (static_cast<float>(i) - static_cast<float>(kSphereGridX - 1) * 0.5f) * kSphereSpacing;
            const float z = (static_cast<float>(j) - static_cast<float>(kSphereGridZ - 1) * 0.5f) * kSphereSpacing;

            auto& sphere = createRenderNode(fmt::format("sphere_{}_{}", i, j));
            sphere.geometry = &sphereGeometry;
            sphere.transformPack->M = glm::translate(glm::vec3(x, 2.0f, z)) * glm::scale(glm::vec3(2.0f));
            sphere.pass(kForwardLightingPass).material = m_material;
            addDepthPrepassEntry(sphere, sphereGeometry);
        }
    }

    m_renderer->getDevice().flushDescriptorUpdates();
}

void ClusteredLightingScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](const Key key, int) {
        switch (key) // NOLINT
        {
        case Key::F5: {
            m_resourceContext->recreatePipelines();
            break;
        }
        default: {
        }
        }
    }));
}

} // namespace crisp
