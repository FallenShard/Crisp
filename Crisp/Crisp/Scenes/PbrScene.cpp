#include <Crisp/Scenes/PbrScene.hpp>

#include <algorithm>

#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("PbrScene");

constexpr uint32_t kShadowMapSize = 4096;
constexpr float kFloorHeight = -1.0f;

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
    std::vector<DrawCommand>& drawCommands,
    const RenderNode& renderNode,
    const std::string_view renderPass,
    const bool includeInvisible = false) {
    if (!includeInvisible && !renderNode.isVisible) {
        return;
    }

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
    const DrawCommand& command,
    const VulkanCommandEncoder& commandEncoder,
    DrawCommandRecordingState& state) {
    if (state.pipeline != command.pipeline) {
        commandEncoder.bindPipeline(*command.pipeline);
        state.pipeline = command.pipeline;
    }
    if ((command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Viewport) && command.viewport.width != 0.0f) {
        commandEncoder.setViewport(command.viewport);
    }
    if ((command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Scissor) && command.scissor.extent.width != 0) {
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

PbrScene::PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_cameraController->setOrbitDistance(1.0f);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    m_renderGraph = std::make_unique<rg::RenderGraph>();

    addCascadedShadowMapPasses(
        *m_renderGraph, kShadowMapSize, [this](const FrameContext& ctx, const uint32_t cascadeIndex) {
            const auto& drawCommands = m_drawCommandCache[cascadeIndex];
            const uint32_t nodeCount =
                std::min(static_cast<uint32_t>(m_nodesToDraw), static_cast<uint32_t>(m_renderNodes.size()));

            const auto& shadowPipelineLayout =
                *m_resourceContext->getMaterial(createShadowMaterialKey(cascadeIndex, "Opaque"))
                     ->getPipeline()
                     ->getPipelineLayout();
            auto& bindlessRegistry = m_renderer->getBindlessImageRegistry();
            CRISP_CHECK_EQ(
                shadowPipelineLayout.getDescriptorSetLayout(BindlessImageRegistry::kGlobalSetIndex),
                bindlessRegistry.getSetLayout(),
                "The PBR shadow pipelines must expose the global bindless layout at set 0.");
            bindlessRegistry.bind(
                ctx.commandEncoder,
                shadowPipelineLayout.getHandle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                BindlessImageRegistry::kGlobalSetIndex);

            DrawCommandRecordingState recordingState{};
            for (const auto& cached : drawCommands) {
                if (cached.nodeIndex >= nodeCount) {
                    continue;
                }
                if (!cached.renderNode->isVisible) {
                    continue;
                }
                if (!m_lightSystem->isCascadeCasterVisible(cascadeIndex, cached.worldBounds)) {
                    continue;
                }
                executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
            }
        });

    addForwardLightingPass(*m_renderGraph, [this](const FrameContext& ctx) {
        const auto& drawCommands = m_drawCommandCache.back();
        const uint32_t nodeCount =
            std::min(static_cast<uint32_t>(m_nodesToDraw), static_cast<uint32_t>(m_renderNodes.size()));
        const auto& pbrPipelineLayout = *m_forwardPassMaterial->getPipeline()->getPipelineLayout();
        auto& bindlessRegistry = m_renderer->getBindlessImageRegistry();
        CRISP_CHECK_EQ(
            pbrPipelineLayout.getDescriptorSetLayout(BindlessImageRegistry::kGlobalSetIndex),
            bindlessRegistry.getSetLayout(),
            "The forward PBR pipeline must expose the global bindless layout at set 0.");
        bindlessRegistry.bind(
            ctx.commandEncoder,
            pbrPipelineLayout.getHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            BindlessImageRegistry::kGlobalSetIndex);
        ctx.commandEncoder.bindDescriptorSets(m_forwardPassMaterial->getDescriptorSetBinding());
        DrawCommandRecordingState recordingState{};
        for (const auto& cached : drawCommands) {
            if (cached.nodeIndex >= nodeCount) {
                break;
            }
            if (!cached.renderNode->isVisible) {
                continue;
            }
            CRISP_CHECK_EQ(
                cached.command.pipeline->getPipelineLayout(),
                &pbrPipelineLayout,
                "Every draw in the bindless PBR batch must use its pipeline layout; draw special pipelines afterward.");
            executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
        }

        std::vector<DrawCommand> specialDrawCommands{};
        createDrawCommand(specialDrawCommands, m_skybox->getRenderNode(), kForwardLightingPass);
        for (const auto& drawCommand : specialDrawCommands) {
            executeDrawCommand(drawCommand, ctx.commandEncoder, recordingState);
        }

        if (m_drawMeshlets) {
            auto* meshPipeline = m_resourceContext->pipelineCache.getPipeline("mesh");
            ctx.commandEncoder.bindPipeline(*meshPipeline);
            auto* meshMaterial = m_resourceContext->getMaterial("mesh");
            ctx.commandEncoder.bindDescriptorSets(meshMaterial->getDescriptorSetBinding());
            ctx.commandEncoder.drawMeshTasks(static_cast<uint32_t>(m_meshletData.meshlets.size()));
        }
    });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        kShadowMapSize,
        kDefaultCascadeCount);
    m_cascadeBlendFraction = args.value("cascadeBlendFraction", m_cascadeBlendFraction);
    m_casterDepthExtrusion = args.value("casterDepthExtrusion", m_casterDepthExtrusion);
    m_visualizeCascades = args.value("visualizeCascades", m_visualizeCascades);
    m_lightSystem->setCascadeBlendFraction(m_cascadeBlendFraction);
    m_lightSystem->setCasterDepthExtrusion(m_casterDepthExtrusion);
    m_lightSystem->setVisualizeCascades(m_visualizeCascades);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaximumObjectCount);

    createCommonTextures();

    for (uint32_t i = 0; i < kCsmPasses.size(); ++i) {
        for (const auto& variant : kShadowMaterialVariants) {
            const std::string key = createShadowMaterialKey(i, variant.suffix);
            auto* csmPipeline = m_resourceContext->createPipeline(
                key, variant.pipelineConfig, m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[i]));
            auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
            csmMaterial->writeDescriptor(1, 0, m_transformBuffer->getDescriptorInfo());
            csmMaterial->writeDescriptor(1, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(i));
        }
    }

    createPlane();
    createSceneObjects(args.value("modelPath", std::string{}));

    if (args.value("meshletTest", false)) {
        createMeshletTestNode();
        m_drawMeshlets = true;
    }

    m_nodesToDraw = static_cast<int32_t>(m_renderNodes.size());
    rebuildDrawCommandCache();

    for (const auto& dir :
         std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps")) {
        m_environmentMapNames.push_back(dir.path().stem().string());
    }
}

void PbrScene::resize(const int32_t width, const int32_t height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());
}

void PbrScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    m_transformBuffer->update(camParams.V, camParams.P);
}

void PbrScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("PbrScene::render", frameContext.commandEncoder);

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

void PbrScene::drawGui() {
    drawCameraPivot(*m_cameraController);

    ImGui::Begin("Scene");
    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraControllerUi(*m_cameraController, /*isSeparateWindow=*/false);
        drawCameraUi(m_cameraController->getCamera(), /*isSeparateWindow=*/false);
    }
    if (ImGui::CollapsingHeader("Light")) {
        DirectionalLight light = m_lightSystem->getDirectionalLight();
        glm::vec3 direction = light.getDirection();
        const bool directionChanged =
            ImGui::SliderFloat("Direction X", &direction.x, -1.0, 1.0) |
            ImGui::SliderFloat("Direction Y", &direction.y, -1.0, 1.0) |
            ImGui::SliderFloat("Direction Z", &direction.z, -1.0, 1.0);
        if (directionChanged && glm::length2(direction) > 0.0f) {
            light.setDirection(direction);
            m_lightSystem->setDirectionalLight(light);
        }

        if (ImGui::SliderFloat("Cascade Blend", &m_cascadeBlendFraction, 0.0f, 0.3f, "%.3f")) {
            m_lightSystem->setCascadeBlendFraction(m_cascadeBlendFraction);
        }
        if (ImGui::SliderFloat("Caster Extrusion", &m_casterDepthExtrusion, 0.0f, 200.0f, "%.1f")) {
            m_lightSystem->setCasterDepthExtrusion(m_casterDepthExtrusion);
        }
        if (ImGui::Checkbox("Visualize Cascades", &m_visualizeCascades)) {
            m_lightSystem->setVisualizeCascades(m_visualizeCascades);
        }

        gui::drawComboBox(
            "Environment Light",
            m_lightSystem->getEnvironmentLight()->getName(),
            m_environmentMapNames,
            [this](const std::string& selectedItem) { setEnvironmentMap(selectedItem); });
    }
    if (ImGui::CollapsingHeader("Objects")) {
        if (ImGui::Checkbox("Show Floor", &m_showFloor)) {
            m_renderNodes["floor"]->isVisible = m_showFloor;
        }
        if (!m_meshletData.meshlets.empty()) {
            ImGui::Checkbox("Draw Meshlets", &m_drawMeshlets);
        }
    }
    ImGui::End();

    ImGui::Begin("Render Graph");
    if (ImGui::CollapsingHeader("Overview")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    if (ImGui::CollapsingHeader("Nodes")) {
        ImGui::SliderInt("Nodes to Draw", &m_nodesToDraw, 0, static_cast<int32_t>(m_renderNodes.size()));
    }
    ImGui::End();
}

void PbrScene::rebuildDrawCommandCache() {
    for (auto& cache : m_drawCommandCache) {
        cache.clear();
        cache.reserve(m_renderNodes.size());
    }

    std::vector<DrawCommand> commands;
    for (uint32_t nodeIndex = 0; const auto& [id, renderNode] : m_renderNodes) {
        for (size_t passIndex = 0; passIndex < m_drawCommandCache.size(); ++passIndex) {
            commands.clear();
            createDrawCommand(
                commands,
                *renderNode,
                passIndex < kCsmPasses.size() ? kCsmPasses[passIndex] : kForwardLightingPass,
                /*includeInvisible=*/true);
            auto& cache = m_drawCommandCache[passIndex];
            for (auto& command : commands) {
                const auto boundsIt = m_renderNodeWorldBounds.find(renderNode.get());
                cache.push_back({
                    .renderNode = renderNode.get(),
                    .nodeIndex = nodeIndex,
                    .worldBounds = boundsIt != m_renderNodeWorldBounds.end() ? boundsIt->second : BoundingBox3{},
                    .command = std::move(command),
                });
            }
        }
        ++nodeIndex;
    }

    // Variant pipelines are shared by many objects; grouping the shadow commands avoids switching among the
    // opaque, alpha-mask, and double-sided states for every node.
    for (size_t passIndex = 0; passIndex < kCsmPasses.size(); ++passIndex) {
        std::ranges::stable_sort(m_drawCommandCache[passIndex], {}, [](const CachedDrawCommand& cached) {
            return cached.command.pipeline;
        });
    }
}

RenderNode& PbrScene::createRenderNode(const std::string_view id, const bool hasTransform) {
    if (!hasTransform) {
        return *m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second;
    }

    const auto transformHandle{m_transformBuffer->getNextIndex()};
    return *m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle)).first->second;
}

void PbrScene::createCommonTextures() {
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    addPbrImageGroupToImageCache(createDefaultPbrImageGroup(), imageCache);

    auto pipeline = m_resourceContext->createPipeline(
        "pbr", "PbrTex.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));

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

void PbrScene::setEnvironmentMap(const std::string& envMapName) {
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName).unwrap(),
        envMapName);
    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_lightSystem->getEnvironmentLight()->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));
}

void PbrScene::createSceneObjects(const std::filesystem::path& path) {
    if (path.empty()) {
        CRISP_LOGW("No modelPath in the scene args; rendering the floor only.");
        return;
    }

    const std::filesystem::path absPath{path.is_absolute() ? path : m_renderer->getResourcesPath() / path};
    if (absPath.extension() == ".gltf" || absPath.extension() == ".glb") {
        createGltfSceneObjects(absPath);
    } else {
        createObjSceneObject(absPath);
    }
}

void PbrScene::createGltfSceneObjects(const std::filesystem::path& path) {
    auto [images, models] = loadGltfAsset(path).unwrap();
    CRISP_LOGI("Loaded {} models from {}.", models.size(), path.generic_string());

    // Every loaded image lands in the bindless table here; the per-model params below resolve their slots by key.
    addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    const auto modelName = path.stem().string();
    for (auto&& [idx, model] : std::views::enumerate(models)) {
        addSceneObject(fmt::format("{}_{}", modelName, idx), model.mesh, model.material, model.transform);
    }
}

void PbrScene::createObjSceneObject(const std::filesystem::path& path) {
    const auto mesh = loadTriangleMesh(path).unwrap();

    PbrMaterial material{};
    material.name = path.stem().string();
    material.params.albedo = glm::vec4(0.5f);

    addSceneObject(
        material.name, mesh, material, glm::translate(glm::vec3(0.0f, kFloorHeight - mesh.getBoundingBox().min.y, 0.0f)));
}

void PbrScene::addSceneObject(
    const std::string_view nodeId, const TriangleMesh& mesh, const PbrMaterial& material, const glm::mat4& modelMatrix) {
    auto& geometry = m_resourceContext->addGeometry(nodeId, createGeometry(*m_renderer, mesh, kPbrVertexFormat));

    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.transformPack->M = modelMatrix;
    m_renderNodeWorldBounds.emplace(&node, transformBoundingBox(mesh.getBoundingBox(), modelMatrix));

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material = m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;
    const auto gpuMaterial = createGpuPbrParams(material, m_resourceContext->imageCache);
    const auto materialHandle = m_pbrMaterialTable->add(gpuMaterial);
    const auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle);
    forwardPass.setPushConstants(drawParameters);

    const auto& shadowVariant = getShadowMaterialVariant(gpuMaterial.flags);
    const bool alphaMasked = (gpuMaterial.flags & PbrMaterialAlphaMask) != 0;

    for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
        auto& subpass = node.pass(kCsmPasses[c]);
        subpass.setGeometry(&geometry, 0, alphaMasked ? 2 : 1);
        subpass.material = m_resourceContext->getMaterial(createShadowMaterialKey(c, shadowVariant.suffix));
        subpass.setPushConstants(drawParameters);
        CRISP_CHECK(subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
    }
}

void PbrScene::createPlane() {
    constexpr std::string_view kNodeName{"floor"};
    m_resourceContext->addGeometry(
        kNodeName, createGeometry(*m_renderer, createPlaneMesh(10.0f, 10.0f), kPbrVertexFormat));

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials/Grass"};
    auto [material, images] = loadPbrMaterial(materialPath);
    material.params.uvScale = glm::vec2(10.0f, 10.0f);
    addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    auto& floor = createRenderNode(kNodeName);
    floor.transformPack->M = glm::translate(glm::vec3(0.0f, kFloorHeight, 0.0f));
    floor.geometry = &m_resourceContext->getGeometry(kNodeName);
    auto& forwardPass = floor.pass(kForwardLightingPass);
    forwardPass.material = m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;
    const auto materialHandle = m_pbrMaterialTable->add(createGpuPbrParams(material, m_resourceContext->imageCache));
    forwardPass.setPushConstants(m_pbrMaterialTable->createDrawParameters(materialHandle));

    CRISP_CHECK(
        floor.pass(kForwardLightingPass)
            .material->getPipeline()
            ->getVertexLayout()
            .isSubsetOf(floor.geometry->getVertexLayout()));
}

void PbrScene::createMeshletTestNode() {
    constexpr std::string_view kNodeName{"meshletTest"};

    auto [mesh, materials, meshletData] =
        loadTriangleMeshlets(m_renderer->getResourcesPath() / "Meshes/bunny.obj").unwrap();
    m_meshletData = std::move(meshletData);

    // The mesh shader reads positions and attributes straight out of the vertex buffers, so they need storage usage.
    auto& geometry = m_resourceContext->addGeometry(
        kNodeName, createGeometry(*m_renderer, mesh, kPbrVertexFormat, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

    auto* meshletBuffer = m_resourceContext->createStorageBuffer("meshletBuffer", m_meshletData.meshlets);
    auto* meshletVertices = m_resourceContext->createStorageBuffer("meshletVertices", m_meshletData.meshletVertices);
    auto* meshletTriangles = m_resourceContext->createStorageBuffer("meshletTriangles", m_meshletData.meshletTriangles);

    auto* meshPipeline = m_resourceContext->createPipeline(
        "mesh", "MeshShading.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    auto* meshMaterial = m_resourceContext->createMaterial("mesh", meshPipeline);
    meshMaterial->writeDescriptor(0, 0, meshletBuffer->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 1, meshletTriangles->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 2, meshletVertices->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 3, geometry.getVertexBuffer(0)->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 4, m_resourceContext->getRingBuffer("camera")->getDescriptorInfo());
    meshMaterial->writeDescriptor(0, 5, geometry.getVertexBuffer(1)->createDescriptorInfo());
}

void PbrScene::setupInput() {
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
