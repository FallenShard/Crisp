#include <Crisp/Scenes/PbrScene.hpp>

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

constexpr uint32_t kShadowMapSize = 1024;

void createDrawCommand(
    std::vector<DrawCommand>& drawCommands, const RenderNode& renderNode, const std::string_view renderPass) {
    if (!renderNode.isVisible) {
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

void executeDrawCommand(
    const DrawCommand& command, const Renderer& renderer, const VulkanCommandEncoder& commandEncoder) {
    commandEncoder.bindPipeline(*command.pipeline);
    if (command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Viewport) {
        commandEncoder.setViewport(command.viewport.width != 0.0f ? command.viewport : renderer.getDefaultViewport());
    }
    if (command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Scissor) {
        commandEncoder.setScissor(command.scissor.extent.width != 0 ? command.scissor : renderer.getDefaultScissor());
    }

    command.pipeline->getPipelineLayout()->setPushConstants(
        commandEncoder.getHandle(), static_cast<const char*>(command.pushConstantView.data));

    if (command.material) {
        command.material->bind(commandEncoder.getHandle(), command.dynamicBufferOffsets);
    }

    command.geometry->bindVertexBuffers(commandEncoder.getHandle(), command.firstBuffer, command.bufferCount);
    command.drawFunc(commandEncoder.getHandle(), command.geometryView);
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    m_rg = std::make_unique<rg::RenderGraph>();

    addCascadedShadowMapPasses(*m_rg, kShadowMapSize, [this](const FrameContext& ctx, const uint32_t cascadeIndex) {
        std::vector<DrawCommand> drawCommands{};
        for (int32_t idx = 0; const auto& [id, renderNode] : m_renderNodes.values()) {
            if (idx++ >= m_nodesToDraw) {
                break;
            }
            createDrawCommand(drawCommands, *renderNode, kCsmPasses[cascadeIndex]);
        }

        for (const auto& drawCommand : drawCommands) {
            executeDrawCommand(drawCommand, *m_renderer, ctx.commandEncoder);
        }
    });

    addForwardLightingPass(*m_rg, [this](const FrameContext& ctx) {
        std::vector<DrawCommand> drawCommands{};
        for (int32_t idx = 0; const auto& [id, renderNode] : m_renderNodes) {
            if (idx++ >= m_nodesToDraw) {
                break;
            }
            createDrawCommand(drawCommands, *renderNode, kForwardLightingPass);
        }
        createDrawCommand(drawCommands, m_skybox->getRenderNode(), kForwardLightingPass);

        m_forwardPassMaterial->bind(ctx.commandEncoder.getHandle());
        for (const auto& drawCommand : drawCommands) {
            executeDrawCommand(drawCommand, *m_renderer, ctx.commandEncoder);
        }
    });

    m_rg->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    updateSceneViews();

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        kShadowMapSize,
        kDefaultCascadeCount);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaximumObjectCount);

    createCommonTextures();

    for (uint32_t i = 0; i < kCsmPasses.size(); ++i) {
        const std::string key = fmt::format("cascadedShadowMap{}", i);
        auto* csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_rg->getRenderPass(kCsmPasses[i]), 0);
        auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(i));
    }

    createPlane();
    createSceneObject(args["modelPath"]);

    for (const auto& dir :
         std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps")) {
        m_environmentMapNames.push_back(dir.path().stem().string());
    }
}

void PbrScene::resize(const int32_t width, const int32_t height) {
    m_cameraController->onViewportResized(width, height);

    m_rg->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    updateRenderPassMaterials();
    updateSceneViews();
}

void PbrScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    m_transformBuffer->update(camParams.V, camParams.P);
}

void PbrScene::render(const FrameContext& frameContext) {
    frameContext.commandEncoder.insertBarrier((kVertexUniformRead | kFragmentUniformRead) >> kTransferWrite);

    const auto& camParams = m_cameraController->getCameraParameters();
    m_lightSystem->update(m_cameraController->getCamera(), 0.0f, frameContext.virtualFrameIndex);
    m_skybox->updateTransforms(camParams.V, camParams.P, frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(camParams, frameContext.virtualFrameIndex);
    m_transformBuffer->updateStagingBuffer(frameContext.virtualFrameIndex);

    m_lightSystem->getCascadedDirectionalLightBuffer()->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder.getHandle());

    frameContext.commandEncoder.insertBarrier(kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead));

    m_rg->execute(frameContext);
}

void PbrScene::drawGui() {
    ImGui::Begin("Scene");
    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraUi(m_cameraController->getCamera(), /*isSeparateWindow=*/false);
    }
    if (ImGui::CollapsingHeader("Light")) {
        DirectionalLight light = m_lightSystem->getDirectionalLight();
        glm::vec3 direction = light.getDirection();
        ImGui::SliderFloat("Direction X", &direction.x, -1.0, 1.0);
        ImGui::SliderFloat("Direction Y", &direction.y, -1.0, 1.0);
        ImGui::SliderFloat("Direction Z", &direction.z, -1.0, 1.0);
        light.setDirection(glm::normalize(direction));
        m_lightSystem->setDirectionalLight(light);
    }
    ImGui::End();

    ImGui::Begin("Render Graph");
    if (ImGui::CollapsingHeader("Overview")) {
        ::crisp::drawGui(*m_rg);
    }
    if (ImGui::CollapsingHeader("Nodes")) {
        ImGui::SliderInt("Nodes to Draw", &m_nodesToDraw, 0, static_cast<int32_t>(m_renderNodes.size()));
    }
    ImGui::End();

    // ImGui::Begin("Settings");
    // ImGui::SliderFloat("Roughness", &m_uniformMaterialParams.roughness,
    // 0.0f, 1.0f); ImGui::SliderFloat("Metallic",
    // &m_uniformMaterialParams.metallic, 0.0f, 1.0f); ImGui::SliderFloat("Red",
    // &m_uniformMaterialParams.albedo.r, 0.0f, 1.0f); ImGui::SliderFloat("Green",
    // &m_uniformMaterialParams.albedo.g, 0.0f, 1.0f); ImGui::SliderFloat("Blue",
    // &m_uniformMaterialParams.albedo.b, 0.0f, 1.0f); ImGui::SliderFloat("U
    // Scale", &m_uniformMaterialParams.uvScale.s, 1.0f, 20.0f);
    // ImGui::SliderFloat("V Scale",
    // &m_uniformMaterialParams.uvScale.t, 1.0f, 20.0f);

    gui::drawComboBox(
        "Environment Light",
        m_lightSystem->getEnvironmentLight()->getName(),
        m_environmentMapNames,
        [this](const std::string& selectedItem) { setEnvironmentMap(selectedItem); });

    // if (ImGui::Checkbox("Show Floor", &m_showFloor)) {
    //     m_renderNodes["floor"]->isVisible = m_showFloor;
    // }

    // ImGui::End();

    // std::vector<std::string> materials;
    // for (const auto& dir :
    //      std::filesystem::directory_iterator(m_renderer->getResourcesPath() /
    //      "Textures/PbrMaterials"))
    //     materials.push_back(dir.path().stem().string());
    // materials.push_back("Uniform");

    // auto comboBox = std::make_unique<gui::ComboBox>(form);
    // comboBox->setId("materialComboBox");
    // comboBox->setItems(materials);
    // comboBox->itemSelected.subscribe<&PbrScene::onMaterialSelected>(this);
    // panel->addControl(std::move(comboBox));

    // form->add(std::move(panel));
}

void PbrScene::onMaterialSelected(const std::string& materialName) {
    // m_renderer->finish();

    // const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / materialName};
    // PbrMaterial pbrMaterial{};
    // pbrMaterial.name = materialPath.stem().string();

    // auto* shaderBallMaterial = m_resourceContext->getMaterial("pbrTexshaderBall");

    // auto& imageCache{m_resourceContext->imageCache};
    // const auto setMaterialTexture =
    //     [&shaderBallMaterial, &imageCache, &pbrMaterial](const uint32_t index, const std::string_view texName) {
    //         const std::string key = fmt::format("{}-{}", pbrMaterial.name, texName);
    //         const std::string fallbackKey = fmt::format("default-{}", texName);
    //         shaderBallMaterial->writeDescriptor(
    //             2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
    //     };

    // // if (pbrMaterial.name != "Grass") {
    // //     auto [material, images] = loadPbrMaterial(materialPath);
    // //     material.params.uvScale = glm::vec2(100.0f, 100.0f);
    // //     addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    // //     addPbrTexturesToImageCache(loadPbrTextureGroup(materialPath), pbrMaterial.name, imageCache);
    // // }
    // // if (m_shaderBallPbrMaterialKey != "Grass") {
    // //     removePbrTexturesFromImageCache(m_shaderBallPbrMaterialKey, imageCache);
    // // }
    // setMaterialTexture(0, "diffuse");
    // setMaterialTexture(1, "metallic");
    // setMaterialTexture(2, "roughness");
    // setMaterialTexture(3, "normal");
    // setMaterialTexture(4, "ao");
    // setMaterialTexture(5, "emissive");
    // m_renderer->getDevice().flushDescriptorUpdates();

    // m_shaderBallPbrMaterialKey = pbrMaterial.name;
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

    auto pipeline =
        m_resourceContext->createPipeline("pbr", "PbrTex.json", m_rg->getRenderPass(kForwardLightingPass), 0);

    setEnvironmentMap("GreenwichPark");
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    imageCache.addImageWithView("sheenLut", createSheenLookup(*m_renderer, m_renderer->getResourcesPath()));

    m_forwardPassMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 0, 1);
    setPbrMaterialSceneParams(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_rg);
}

void PbrScene::setEnvironmentMap(const std::string& envMapName) {
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName).unwrap(),
        envMapName);
    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_rg->getRenderPass(kForwardLightingPass),
        m_lightSystem->getEnvironmentLight()->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));
}

void PbrScene::createSceneObject(const std::filesystem::path& path) {
    const std::filesystem::path absPath{path.is_absolute() ? path : m_renderer->getResourcesPath() / path};
    if (absPath.extension() == ".gltf") {
        auto [images, models] = loadGltfAsset(absPath).unwrap();
        CRISP_LOGI("Loaded {} models and {} images from {}.", models.size(), images.size(), absPath.generic_string());
        addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

        for (auto&& [idx, renderObject] : std::views::enumerate(models)) {
            // const std::string materialName = fmt::format("material_{}", idx);
            const std::string entityName = fmt::format("renderObject_{}", idx);
            m_shaderBallPbrMaterialKey = fmt::format("material_{}", idx);

            CRISP_LOGI("Adding object {} with {} triangles.", idx, renderObject.mesh.getTriangleCount());

            auto& geometry = m_resourceContext->addGeometry(
                entityName, createGeometry(*m_renderer, renderObject.mesh, kPbrVertexFormat));

            auto& sceneObject = createRenderNode(entityName);
            sceneObject.geometry = &geometry;
            sceneObject.transformPack->M = renderObject.transform;
            sceneObject.pass(kForwardLightingPass).material =
                createPbrMaterial(entityName, renderObject.material, *m_resourceContext, *m_transformBuffer);
            sceneObject.pass(kForwardLightingPass).transformBufferDynamicIndex = 0;
            sceneObject.pass(kForwardLightingPass).material->setBindRange(1, 2, 0, 1);
            m_renderer->getDevice().flushDescriptorUpdates();

            for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
                auto& subpass = sceneObject.pass(kCsmPasses[c]);
                subpass.setGeometry(&geometry, 0, 1);
                subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
                CRISP_CHECK(
                    subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
            }
        }

        m_renderer->getDevice().flushDescriptorUpdates();

        // const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
        // sceneObject->transformPack->M = translation;
        // translation * glm::rotate(glm::pi<float>() * 0.5f, glm ::vec3(1.0f, 0.0f, 0.0f));
    }

    // else {
    //     const std::string entityName{"sceneObject"};
    //     auto& sceneObject = createRenderNode(entityName, true);
    //     TriangleMesh mesh{};
    //     PbrMaterial material{};

    //     auto [triMesh, materials] =
    //         loadTriangleMeshAndMaterial(
    //             m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.obj", flatten(kPbrVertexFormat))
    //             .unwrap();
    //     mesh = std::move(triMesh);

    //     // material.name = "vokselia";
    //     // material.textureKeys = createDefaultPbrTextureGroup();
    //     // material.textures.albedo =
    //     //     loadImage(m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.png", 4, FlipAxis::Y).unwrap();

    //     const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
    //     const float maxDimLength = mesh.getBoundingBox().getMaximumExtent();
    //     sceneObject->transformPack->M = translation * glm::scale(glm::vec3(10.0f / maxDimLength));
    // }

    // m_nodesToDraw = static_cast<int32_t>(m_renderNodes.size());

    // m_shaderBallPbrMaterialKey = material.name;
    // addPbrTexturesToImageCache(material.textures, material.name, m_resourceContext->imageCache);

    // m_resourceContext->addGeometry(entityName, createGeometry(*m_renderer, mesh, kPbrVertexFormat));
    // sceneObject->geometry = m_resourceContext->getGeometry(entityName);
    // sceneObject->pass(kForwardLightingPass).material =
    //     createPbrMaterial(entityName, material.name, *m_resourceContext, material.params, *m_transformBuffer);
    // setPbrMaterialSceneParams(
    //     *sceneObject->pass(kForwardLightingPass).material, *m_resourceContext, *m_lightSystem, *m_rg);
    // m_renderer->getDevice().flushDescriptorUpdates();

    // for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
    //     auto& subpass = sceneObject->pass(kCsmPasses[c]);
    //     subpass.setGeometry(m_resourceContext->getGeometry(entityName), 0, 1);
    //     subpass.material = m_resourceContext->getMaterial("cascadedShadowMap" + std::to_string(c));
    //     CRISP_CHECK(subpass.material->getPipeline()->getVertexLayout().isSubsetOf(subpass.geometry->getVertexLayout()));
    // }

    // m_renderer->getDevice().flushDescriptorUpdates();
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
    floor.transformPack->M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f));
    floor.geometry = &m_resourceContext->getGeometry(kNodeName);
    floor.pass(kForwardLightingPass).material =
        createPbrMaterial(kNodeName, material, *m_resourceContext, *m_transformBuffer);
    floor.pass(kForwardLightingPass).transformBufferDynamicIndex = 0;
    floor.pass(kForwardLightingPass).material->setBindRange(1, 2, 0, 1);

    // CRISP_CHECK(
    //     floor->pass(kForwardLightingPass)
    //         .material->getPipeline()
    //         ->getVertexLayout()
    //         .isSubsetOf(floor->geometry->getVertexLayout()));
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

void PbrScene::updateRenderPassMaterials() {
    setPbrMaterialSceneParams(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_rg);
}

void PbrScene::updateSceneViews() {
    const auto& data = m_rg->getBlackboard().get<ForwardLightingData>();
    m_renderer->setSceneImageView(&m_rg->getResourceImageView(data.hdrImage));
}

} // namespace crisp