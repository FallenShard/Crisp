#include <Crisp/Scenes/GltfViewerScene.hpp>

#include <algorithm>
#include <ranges>

#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("GltfViewerScene");

constexpr uint32_t kShadowMapSize = 4096;

struct ShadowMaterialVariant {
    std::string_view suffix;
    std::string_view pipelineConfig;
};

constexpr std::array kShadowMaterialVariants{
    ShadowMaterialVariant{"Opaque", "PbrShadowMap.json"},
    ShadowMaterialVariant{"DoubleSided", "PbrShadowMapDoubleSided.json"},
    ShadowMaterialVariant{"Alpha", "PbrShadowMapAlpha.json"},
    ShadowMaterialVariant{"AlphaDoubleSided", "PbrShadowMapAlphaDoubleSided.json"},
};

const ShadowMaterialVariant& getShadowMaterialVariant(const uint32_t materialFlags) {
    const bool alphaMasked = (materialFlags & PbrMaterialAlphaMask) != 0;
    const bool doubleSided = (materialFlags & PbrMaterialDoubleSided) != 0;
    return kShadowMaterialVariants[static_cast<size_t>(alphaMasked) * 2 + static_cast<size_t>(doubleSided)];
}

std::string createShadowMaterialKey(const uint32_t cascadeIndex, const std::string_view suffix) {
    return fmt::format("cascadedShadowMap{}{}", cascadeIndex, suffix);
}

BoundingBox3 transformBoundingBox(const BoundingBox3& localBounds, const glm::mat4& transform) {
    BoundingBox3 worldBounds;
    for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        worldBounds.expandBy(glm::vec3(transform * glm::vec4(localBounds.getCorner(cornerIndex), 1.0f)));
    }
    return worldBounds;
}

void createDrawCommand(
    std::vector<DrawCommand>& drawCommands, const RenderNode& renderNode, const std::string_view renderPass) {
    for (const auto& [key, materialMap] : renderNode.materials) {
        if (key.renderPassName != renderPass) {
            continue;
        }
        for (const auto& [part, material] : materialMap) {
            drawCommands.push_back(material.createDrawCommand(renderNode));
        }
    }
}

struct DrawCommandRecordingState {
    const VulkanPipeline* pipeline{nullptr};
};

void executeDrawCommand(
    const DrawCommand& command, const VulkanCommandEncoder& commandEncoder, DrawCommandRecordingState& state) {
    if (state.pipeline != command.pipeline) {
        commandEncoder.bindPipeline(*command.pipeline);
        state.pipeline = command.pipeline;
    }
    if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Viewport) &&
        command.viewport.width != 0.0f) {
        commandEncoder.setViewport(command.viewport);
    }
    if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Scissor) &&
        command.scissor.extent.width != 0) {
        commandEncoder.setScissor(command.scissor);
    }

    commandEncoder.setPushConstants(*command.pipeline->getPipelineLayout(), command.pushConstantView.asSpan());

    if (command.material) {
        commandEncoder.bindDescriptorSets(command.material->getDescriptorSetBinding(command.dynamicBufferOffsets));
    }

    command.geometry->bindVertexBuffers(commandEncoder, command.firstBuffer, command.bufferCount);
    command.drawFunc(commandEncoder, command.geometryView);
}
} // namespace

GltfViewerScene::GltfViewerScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_rayTracedShadowsSupported = m_renderer->getDevice().getEnabledFeatures().rayQuery;

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_cameraController->setOrbitDistance(1.0f);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    m_renderGraph = std::make_unique<rg::RenderGraph>();

    addCascadedShadowMapPasses(
        *m_renderGraph, kShadowMapSize, [this](const FrameContext& ctx, const uint32_t cascadeIndex) {
            const auto& drawCommands = m_drawCommandCache[cascadeIndex];
            const auto& shadowPipelineLayout =
                *m_resourceContext->getMaterial(createShadowMaterialKey(cascadeIndex, "Opaque"))
                     ->getPipeline()
                     ->getPipelineLayout();
            auto& bindlessRegistry = m_renderer->getBindlessImageRegistry();
            bindlessRegistry.bind(
                ctx.commandEncoder,
                shadowPipelineLayout.getHandle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                BindlessImageRegistry::kGlobalSetIndex);

            DrawCommandRecordingState recordingState{};
            for (const auto& cached : drawCommands) {
                if (!m_lightSystem->isCascadeCasterVisible(cascadeIndex, cached.worldBounds)) {
                    continue;
                }
                executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
            }
        });

    addForwardLightingPass(*m_renderGraph, [this](const FrameContext& ctx) {
        const auto& pbrPipelineLayout = *m_forwardPassMaterial->getPipeline()->getPipelineLayout();
        auto& bindlessRegistry = m_renderer->getBindlessImageRegistry();
        bindlessRegistry.bind(
            ctx.commandEncoder,
            pbrPipelineLayout.getHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            BindlessImageRegistry::kGlobalSetIndex);
        ctx.commandEncoder.bindDescriptorSets(m_forwardPassMaterial->getDescriptorSetBinding());

        DrawCommandRecordingState recordingState{};
        for (const auto& cached : m_drawCommandCache.back()) {
            executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
        }

        std::vector<DrawCommand> specialDrawCommands{};
        createDrawCommand(specialDrawCommands, m_skybox->getRenderNode(), kForwardLightingPass);
        for (const auto& drawCommand : specialDrawCommands) {
            executeDrawCommand(drawCommand, ctx.commandEncoder, recordingState);
        }
    });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        kShadowMapSize,
        kDefaultCascadeCount);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaximumObjectCount);

    createCommonTextures();

    for (uint32_t i = 0; i < kCsmPasses.size(); ++i) {
        for (const auto& variant : kShadowMaterialVariants) {
            const std::string key = createShadowMaterialKey(i, variant.suffix);
            auto* csmPipeline = m_resourceContext->createPipeline(
                key, variant.pipelineConfig, {m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[i])});
            auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
            csmMaterial->writeDescriptor(1, 0, m_transformBuffer->getDescriptorInfo());
            csmMaterial->writeDescriptor(1, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(i));
        }
    }

    loadAsset(args.value("modelPath", std::string{}));
    createRayTracedShadowResources();
    rebuildDrawCommandCache();

    for (const auto& dir :
         std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps")) {
        m_environmentMapNames.push_back(dir.path().stem().string());
    }
}

void GltfViewerScene::resize(const int width, const int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());
}

void GltfViewerScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    m_transformBuffer->update(camParams.V, camParams.P);
}

void GltfViewerScene::render(const FrameContext& frameContext) {
    frameContext.commandEncoder.insertBarrier(
        (kVertexUniformRead | kFragmentUniformRead | kFragmentRead) >> kTransferWrite);

    const auto& camParams = m_cameraController->getCameraParameters();
    m_lightSystem->update(m_cameraController->getCamera(), frameContext.virtualFrameIndex);
    m_lightSystem->getCascadedDirectionalLightBuffer()->updateDeviceBuffer(frameContext.commandEncoder);

    m_skybox->updateTransforms(camParams.V, camParams.P, frameContext.virtualFrameIndex);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);

    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(camParams, frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->updateStagingBuffer(frameContext.virtualFrameIndex);
    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
    m_pbrMaterialTable->updateDeviceBuffer(*frameContext.stagingBelt, frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead | kFragmentRead));

    m_renderGraph->execute(frameContext);
}

void GltfViewerScene::drawGui() {
    drawCameraPivot(*m_cameraController);

    ImGui::Begin("glTF Viewer");

    if (m_report.path.empty()) {
        ImGui::TextWrapped("No modelPath in the scene args. Point it at a .gltf or .glb to load one.");
    } else {
        ImGui::TextWrapped("%s", m_report.path.filename().string().c_str()); // NOLINT
        ImGui::Separator();

        if (m_report.succeeded) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Loaded");
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Failed");
            ImGui::TextWrapped("%s", m_report.error.c_str()); // NOLINT
        }
    }

    if (m_report.succeeded && ImGui::CollapsingHeader("Contents", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Models      %u", m_report.modelCount);    // NOLINT
        ImGui::Text("Submeshes   %u", m_report.submeshCount);  // NOLINT
        ImGui::Text("Vertices    %u", m_report.vertexCount);   // NOLINT
        ImGui::Text("Triangles   %u", m_report.triangleCount); // NOLINT
        ImGui::Text("Images      %u", m_report.imageCount);    // NOLINT

        const glm::vec3 extent = m_report.bounds.max - m_report.bounds.min;
        ImGui::Text("Extent      %.3f %.3f %.3f", extent.x, extent.y, extent.z); // NOLINT
    }

    if (m_report.succeeded && ImGui::CollapsingHeader("Feature coverage", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Alpha masked    %u", m_report.alphaMaskedCount);  // NOLINT
        ImGui::Text("Double sided    %u", m_report.doubleSidedCount);  // NOLINT
        ImGui::Text("Skinned models  %u", m_report.skinnedModelCount); // NOLINT
        ImGui::Text("Animations      %u", m_report.animationCount);    // NOLINT

        if (m_report.skinnedModelCount != 0 || m_report.animationCount != 0) {
            ImGui::TextWrapped("Skinning and animation data is parsed but not yet played back.");
        }
    }

    if (m_rayTracedShadowsSupported && m_shadowTlas != nullptr &&
        ImGui::Checkbox("Ray-traced shadows", &m_useRayTracedShadows)) {
        updateForwardDrawParameters();
    }

    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraControllerUi(*m_cameraController, /*isSeparateWindow=*/false);
        drawCameraUi(m_cameraController->getCamera(), /*isSeparateWindow=*/false);
    }

    if (ImGui::CollapsingHeader("Environment")) {
        for (const auto& envMapName : m_environmentMapNames) {
            if (ImGui::Button(envMapName.c_str())) {
                setEnvironmentMap(envMapName);
            }
        }
    }

    ImGui::End();

    drawRenderGraphGui(*m_renderGraph);
}

RenderNode& GltfViewerScene::createRenderNode(const std::string_view id, const bool hasTransform) {
    if (!hasTransform) {
        return *m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second;
    }

    const auto transformHandle{m_transformBuffer->getNextIndex()};
    return *m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle)).first->second;
}

void GltfViewerScene::createCommonTextures() {
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    addPbrImageGroupToImageCache(createDefaultPbrImageGroup(), imageCache);

    auto pipeline = m_resourceContext->createPipeline(
        "pbr", "PbrTex.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});

    setEnvironmentMap("GreenwichPark");
    imageCache.addImage("brdfLut", integrateBrdfLut(m_renderer));

    m_forwardPassMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 1, 1);
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);

    m_pbrMaterialTable = std::make_unique<PbrMaterialTable>(m_renderer->getDevice(), kMaximumObjectCount);
    m_pbrDrawMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 2, 1);
    m_pbrDrawMaterial->writeDescriptor(2, 0, m_transformBuffer->getDescriptorInfo());
}

void GltfViewerScene::setEnvironmentMap(const std::string& envMapName) {
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName).unwrap(),
        envMapName);
    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_lightSystem->getEnvironmentLight()->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));
}

void GltfViewerScene::loadAsset(const std::filesystem::path& path) {
    if (path.empty()) {
        logger->warn("No modelPath in the scene args; nothing to validate.");
        return;
    }

    const std::filesystem::path absPath{path.is_absolute() ? path : m_renderer->getResourcesPath() / path};
    m_report = {};
    m_report.path = absPath;

    if (absPath.extension() != ".gltf" && absPath.extension() != ".glb") {
        m_report.error =
            fmt::format("Not a glTF asset: expected .gltf or .glb, got '{}'.", absPath.extension().string());
        logger->error("{}", m_report.error);
        return;
    }

    auto assetResult = loadGltfAsset(absPath);
    if (!assetResult.hasValue()) {
        m_report.error = assetResult.getError();
        logger->error("Failed to load {}: {}", absPath.generic_string(), m_report.error);
        return;
    }

    auto [images, models] = std::move(assetResult).unwrap();

    addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    m_report.succeeded = true;
    m_report.modelCount = static_cast<uint32_t>(models.size());
    m_report.imageCount = static_cast<uint32_t>(images.size());

    const auto modelName = absPath.stem().string();
    for (auto&& [idx, model] : std::views::enumerate(models)) {
        addSceneObject(fmt::format("{}_{}", modelName, idx), model.mesh, model.material, model.transform);

        m_report.vertexCount += model.mesh.getVertexCount();
        m_report.triangleCount += model.mesh.getTriangleCount();
        m_report.submeshCount += static_cast<uint32_t>(model.mesh.getViews().size());
        m_report.animationCount += static_cast<uint32_t>(model.animations.size());
        if (!model.skinningData.skeleton.joints.empty()) {
            ++m_report.skinnedModelCount;
        }

        const auto gpuFlags = createGpuPbrParams(model.material, m_resourceContext->imageCache).flags;
        if ((gpuFlags & PbrMaterialAlphaMask) != 0) {
            ++m_report.alphaMaskedCount;
        }
        if ((gpuFlags & PbrMaterialDoubleSided) != 0) {
            ++m_report.doubleSidedCount;
        }

        m_report.bounds.expandBy(transformBoundingBox(model.mesh.getBoundingBox(), model.transform));
    }

    logger->info("Loaded {} models from {}.", m_report.modelCount, absPath.generic_string());
    frameCameraOnBounds(m_report.bounds);
}

void GltfViewerScene::addSceneObject(
    const std::string_view nodeId, const TriangleMesh& mesh, const PbrMaterial& material, const glm::mat4& modelMatrix) {
    const VkBufferUsageFlags2 accelerationStructureUsage =
        m_rayTracedShadowsSupported
            ? VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            : 0;
    auto& geometry = m_resourceContext->addGeometry(
        nodeId, createGeometry(*m_renderer, mesh, kPbrVertexFormat, accelerationStructureUsage));

    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.transformPack->M = modelMatrix;
    m_renderNodeWorldBounds.emplace(&node, transformBoundingBox(mesh.getBoundingBox(), modelMatrix));

    if (m_rayTracedShadowsSupported) {
        m_shadowBlases.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                m_renderer->getDevice(),
                createAccelerationStructureGeometry(geometry, 0),
                mesh.getTriangleCount(),
                modelMatrix));
        m_shadowBlases.back()->setDebugName(m_renderer->getDevice(), fmt::format("glTF Viewer {} BLAS", nodeId));
    }

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material = m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;
    const auto gpuMaterial = createGpuPbrParams(material, m_resourceContext->imageCache);
    const auto materialHandle = m_pbrMaterialTable->add(gpuMaterial);
    m_pbrMaterialHandles.emplace(&node, materialHandle);
    const PbrDrawFlagFlags drawFlags =
        m_useRayTracedShadows ? PbrDrawFlagFlags{PbrDrawFlag::RayTracedShadows} : PbrDrawFlagFlags{};
    const auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle, drawFlags);
    forwardPass.setPushConstants(drawParameters);

    const auto& shadowVariant = getShadowMaterialVariant(gpuMaterial.flags);
    const bool alphaMasked = (gpuMaterial.flags & PbrMaterialAlphaMask) != 0;

    for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
        auto& subpass = node.pass(kCsmPasses[c]);
        subpass.setGeometry(&geometry, 0, alphaMasked ? 2 : 1);
        subpass.material = m_resourceContext->getMaterial(createShadowMaterialKey(c, shadowVariant.suffix));
        subpass.setPushConstants(drawParameters);
    }
}

void GltfViewerScene::createRayTracedShadowResources() {
    if (!m_rayTracedShadowsSupported || m_shadowBlases.empty()) {
        return;
    }

    std::vector<VulkanAccelerationStructure*> blases;
    blases.reserve(m_shadowBlases.size());
    for (const auto& blas : m_shadowBlases) {
        blases.push_back(blas.get());
    }

    m_shadowTlas = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);
    m_shadowTlas->setDebugName(m_renderer->getDevice(), "glTF Viewer Shadow TLAS");
    m_forwardPassMaterial->writeDescriptor(1, 6, m_shadowTlas->getDescriptorInfo());

    m_renderer->enqueueResourceUpdate([this](const VulkanCommandEncoder& encoder) {
        for (auto& blas : m_shadowBlases) {
            encoder.buildAccelerationStructure(*blas);
        }
        encoder.insertBarrier(kAccelerationStructureWrite >> kAccelerationStructureRead);
        encoder.buildAccelerationStructure(*m_shadowTlas);
        encoder.insertBarrier(kAccelerationStructureWrite >> kFragmentAccelerationStructureRead);
    });
}

void GltfViewerScene::updateForwardDrawParameters() {
    const PbrDrawFlagFlags drawFlags =
        m_useRayTracedShadows ? PbrDrawFlagFlags{PbrDrawFlag::RayTracedShadows} : PbrDrawFlagFlags{};
    for (const auto& [node, materialHandle] : m_pbrMaterialHandles) {
        node->pass(kForwardLightingPass)
            .setPushConstants(m_pbrMaterialTable->createDrawParameters(materialHandle, drawFlags));
    }
    rebuildDrawCommandCache();
}

void GltfViewerScene::frameCameraOnBounds(const BoundingBox3& bounds) {
    const glm::vec3 extent{bounds.max - bounds.min};
    const float radius = 0.5f * glm::length(extent);
    if (radius <= 0.0f) {
        return;
    }

    m_cameraController->setTarget(0.5f * (bounds.min + bounds.max));
    m_cameraController->setOrbitDistance(2.5f * radius);
}

void GltfViewerScene::rebuildDrawCommandCache() {
    for (auto& cache : m_drawCommandCache) {
        cache.clear();
        cache.reserve(m_renderNodes.size());
    }

    std::vector<DrawCommand> commands;
    for (const auto& [id, renderNode] : m_renderNodes) {
        for (size_t passIndex = 0; passIndex < m_drawCommandCache.size(); ++passIndex) {
            commands.clear();
            createDrawCommand(
                commands, *renderNode, passIndex < kCsmPasses.size() ? kCsmPasses[passIndex] : kForwardLightingPass);
            auto& cache = m_drawCommandCache[passIndex];
            for (auto& command : commands) {
                const auto boundsIt = m_renderNodeWorldBounds.find(renderNode.get());
                cache.push_back({
                    .renderNode = renderNode.get(),
                    .worldBounds = boundsIt != m_renderNodeWorldBounds.end() ? boundsIt->second : BoundingBox3{},
                    .command = std::move(command),
                });
            }
        }
    }

    for (size_t passIndex = 0; passIndex < kCsmPasses.size(); ++passIndex) {
        std::ranges::stable_sort(m_drawCommandCache[passIndex], {}, [](const CachedDrawCommand& cached) {
            return cached.command.pipeline;
        });
    }
}

void GltfViewerScene::setupInput() {
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
