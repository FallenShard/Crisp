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

void executeDrawCommand(
    const DrawCommand& command,
    const Renderer& renderer,
    const VulkanCommandBuffer& cmdBuffer,
    const uint32_t virtualFrameIndex) {
    command.pipeline->bind(cmdBuffer.getHandle());
    if (command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Viewport) {
        if (command.viewport.width != 0.0f) {
            vkCmdSetViewport(cmdBuffer.getHandle(), 0, 1, &command.viewport);
        } else {
            renderer.setDefaultViewport(cmdBuffer.getHandle());
        }
    }
    if (command.pipeline->getDynamicStateFlags() & PipelineDynamicState::Scissor) {
        if (command.scissor.extent.width != 0) {
            vkCmdSetScissor(cmdBuffer.getHandle(), 0, 1, &command.scissor);
        } else {
            renderer.setDefaultScissor(cmdBuffer.getHandle());
        }
    }

    command.pipeline->getPipelineLayout()->setPushConstants(
        cmdBuffer.getHandle(), static_cast<const char*>(command.pushConstantView.data));

    if (command.material) {
        command.material->bind(virtualFrameIndex, cmdBuffer.getHandle(), command.dynamicBufferOffsets);
    }

    command.geometry->bindVertexBuffers(cmdBuffer.getHandle(), command.firstBuffer, command.bufferCount);
    command.drawFunc(cmdBuffer.getHandle(), command.geometryView);
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);

    m_rg = std::make_unique<rg::RenderGraph>();

    addCascadedShadowMapPasses(
        *m_rg, kShadowMapSize, [this](const RenderPassExecutionContext& ctx, const uint32_t cascadeIndex) {
            std::vector<DrawCommand> drawCommands{};
            for (int32_t idx = 0; const auto& [id, renderNode] : m_renderNodes.values()) {
                if (idx++ >= m_nodesToDraw) {
                    break;
                }
                createDrawCommand(drawCommands, *renderNode, kCsmPasses[cascadeIndex], ctx.virtualFrameIndex);
            }

            for (const auto& drawCommand : drawCommands) {
                executeDrawCommand(drawCommand, *m_renderer, ctx.cmdBuffer, ctx.virtualFrameIndex);
            }
        });

    addForwardLightingPass(*m_rg, [this](const RenderPassExecutionContext& ctx) {
        std::vector<DrawCommand> drawCommands{};
        for (int32_t idx = 0; const auto& [id, renderNode] : m_renderNodes) {
            if (idx++ >= m_nodesToDraw) {
                break;
            }
            createDrawCommand(drawCommands, *renderNode, kForwardLightingPass, ctx.virtualFrameIndex);
        }
        createDrawCommand(drawCommands, m_skybox->getRenderNode(), kForwardLightingPass, ctx.virtualFrameIndex);

        m_forwardPassMaterial->bind(ctx.virtualFrameIndex, ctx.cmdBuffer.getHandle());
        for (const auto& drawCommand : drawCommands) {
            executeDrawCommand(drawCommand, *m_renderer, ctx.cmdBuffer, ctx.virtualFrameIndex);
        }
    });

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
        updateSceneViews();
    });
    m_renderer->flushResourceUpdates(true);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::vec3(1, 1, 0), glm::vec3(3.0f), glm::vec3(-5), glm::vec3(5)),
        kShadowMapSize,
        kDefaultCascadeCount);

    // Object transforms
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, 5000);
    m_renderer->getDebugMarker().setObjectName(m_transformBuffer->getUniformBuffer()->get(), "transformBuffer");

    createCommonTextures();

    for (uint32_t i = 0; i < kCsmPasses.size(); ++i) {
        const std::string key = fmt::format("cascadedShadowMap{}", i);
        auto* csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_rg->getRenderPass(kCsmPasses[i]), 0);
        auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    createPlane();
    createSceneObject(args["modelPath"]);

    m_skybox = m_lightSystem->getEnvironmentLight()->createSkybox(
        *m_renderer, m_rg->getRenderPass(kForwardLightingPass), m_resourceContext->imageCache.getSampler("linearClamp"));

    m_renderer->getDevice().flushDescriptorUpdates();

    for (const auto& dir :
         std::filesystem::directory_iterator(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps")) {
        m_environmentMapNames.push_back(dir.path().stem().string());
    }
}

void PbrScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderer->getDevice().postResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
        updateRenderPassMaterials();
        updateSceneViews();
    });
    m_renderer->flushResourceUpdates(true);
}

void PbrScene::update(float dt) {
    // Camera
    m_cameraController->update(dt);
    const auto camParams = m_cameraController->getCameraParameters();
    m_resourceContext->getUniformBuffer("camera")->updateStagingBuffer(camParams);

    // Object transforms
    m_transformBuffer->update(camParams.V, camParams.P);
    m_lightSystem->update(m_cameraController->getCamera(), dt);
    m_skybox->updateTransforms(camParams.V, camParams.P);

    // m_resourceContext->getUniformBuffer("sceneObject-params")->updateStagingBuffer(m_uniformMaterialParams);
}

void PbrScene::render() {
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) {
        m_rg->execute(cmdBuffer, m_renderer->getCurrentVirtualFrameIndex());
    });
}

void PbrScene::renderGui() {
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
        drawGui(*m_rg);
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

    // gui::drawComboBox(
    //     "Environment Light",
    //     m_lightSystem->getEnvironmentLight()->getName(),
    //     m_environmentMapNames,
    //     [this](const std::string& selectedItem) {
    //     setEnvironmentMap(selectedItem); });

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
    m_renderer->finish();

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / materialName};
    PbrMaterial pbrMaterial{};
    pbrMaterial.name = materialPath.stem().string();

    auto* shaderBallMaterial = m_resourceContext->getMaterial("pbrTexshaderBall");

    auto& imageCache{m_resourceContext->imageCache};
    const auto setMaterialTexture =
        [&shaderBallMaterial, &imageCache, &pbrMaterial](const uint32_t index, const std::string_view texName) {
            const std::string key = fmt::format("{}-{}", pbrMaterial.name, texName);
            const std::string fallbackKey = fmt::format("default-{}", texName);
            shaderBallMaterial->writeDescriptor(
                2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
        };

    // if (pbrMaterial.name != "Grass") {
    //     auto [material, images] = loadPbrMaterial(materialPath);
    //     material.params.uvScale = glm::vec2(100.0f, 100.0f);
    //     addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    //     addPbrTexturesToImageCache(loadPbrTextureGroup(materialPath), pbrMaterial.name, imageCache);
    // }
    // if (m_shaderBallPbrMaterialKey != "Grass") {
    //     removePbrTexturesFromImageCache(m_shaderBallPbrMaterialKey, imageCache);
    // }
    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");
    m_renderer->getDevice().flushDescriptorUpdates();

    m_shaderBallPbrMaterialKey = pbrMaterial.name;
}

RenderNode* PbrScene::createRenderNode(const std::string_view id, const bool hasTransform) {
    if (!hasTransform) {
        return m_renderNodes.emplace(id, std::make_unique<RenderNode>()).first->second.get();
    }

    const auto transformHandle{m_transformBuffer->getNextIndex()};
    return m_renderNodes.emplace(id, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle))
        .first->second.get();
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
}

void PbrScene::createSceneObject(const std::filesystem::path& path) {
    const std::filesystem::path absPath{path.is_absolute() ? path : m_renderer->getResourcesPath() / path};
    if (absPath.extension() == ".gltf") {
        auto [images, models] = loadGltfAsset(absPath).unwrap();
        logger->info("Loaded {} models and {} images from {}.", models.size(), images.size(), absPath.generic_string());
        addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

        for (auto&& [idx, renderObject] : std::views::enumerate(models)) {
            const std::string materialName = fmt::format("material_{}", idx);
            const std::string entityName = fmt::format("renderObject_{}", idx);
            m_shaderBallPbrMaterialKey = fmt::format("material_{}", idx);

            logger->info("Adding object {} with {} triangles.", idx, renderObject.mesh.getTriangleCount());

            m_resourceContext->addGeometry(entityName, createFromMesh(*m_renderer, renderObject.mesh, kPbrVertexFormat));

            auto* sceneObject = createRenderNode(entityName, true);
            sceneObject->geometry = m_resourceContext->getGeometry(entityName);
            sceneObject->transformPack->M = renderObject.transform;
            sceneObject->pass(kForwardLightingPass).material =
                createPbrMaterial(entityName, renderObject.material, *m_resourceContext, *m_transformBuffer);
            sceneObject->pass(kForwardLightingPass).transformBufferDynamicIndex = 2;
            sceneObject->pass(kForwardLightingPass).material->setBindRange(1, 2, 2, 1);
            m_renderer->getDevice().flushDescriptorUpdates();

            for (uint32_t c = 0; c < kDefaultCascadeCount; ++c) {
                auto& subpass = sceneObject->pass(kCsmPasses[c]);
                subpass.setGeometry(m_resourceContext->getGeometry(entityName), 0, 1);
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

    else {
        const std::string entityName{"sceneObject"};
        auto* sceneObject = createRenderNode(entityName, true);
        TriangleMesh mesh{};
        PbrMaterial material{};

        auto [triMesh, materials] =
            loadTriangleMeshAndMaterial(
                m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.obj", flatten(kPbrVertexFormat))
                .unwrap();
        mesh = std::move(triMesh);

        // material.name = "vokselia";
        // material.textureKeys = createDefaultPbrTextureGroup();
        // material.textures.albedo =
        //     loadImage(m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.png", 4, FlipAxis::Y).unwrap();

        const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
        const float maxDimLength = mesh.getBoundingBox().getMaximumExtent();
        sceneObject->transformPack->M = translation * glm::scale(glm::vec3(10.0f / maxDimLength));
    }

    m_nodesToDraw = static_cast<int32_t>(m_renderNodes.size());

    // m_shaderBallPbrMaterialKey = material.name;
    // addPbrTexturesToImageCache(material.textures, material.name, m_resourceContext->imageCache);

    // m_resourceContext->addGeometry(entityName, createFromMesh(*m_renderer, mesh, kPbrVertexFormat));
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
        kNodeName, createFromMesh(*m_renderer, createPlaneMesh(10.0f, 10.0f), kPbrVertexFormat));

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials/Grass"};
    auto [material, images] = loadPbrMaterial(materialPath);
    material.params.uvScale = glm::vec2(10.0f, 10.0f);
    addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    auto* floor = createRenderNode(kNodeName, true);
    floor->transformPack->M = glm::translate(glm::vec3(0.0f, -1.0f, 0.0f));
    floor->geometry = m_resourceContext->getGeometry(kNodeName);
    floor->pass(kForwardLightingPass).material =
        createPbrMaterial(kNodeName, material, *m_resourceContext, *m_transformBuffer);
    floor->pass(kForwardLightingPass).transformBufferDynamicIndex = 2;
    floor->pass(kForwardLightingPass).material->setBindRange(1, 2, 2, 1);

    CRISP_CHECK(
        floor->pass(kForwardLightingPass)
            .material->getPipeline()
            ->getVertexLayout()
            .isSubsetOf(floor->geometry->getVertexLayout()));
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
    m_sceneImageViews.resize(kRendererVirtualFrameCount);
    for (auto& sv : m_sceneImageViews) {
        sv = m_rg->createViewFromResource(m_renderer->getDevice(), data.hdrImage);
    }

    m_renderer->setSceneImageViews(m_sceneImageViews);
}

} // namespace crisp