#include <Crisp/Scenes/PbrScene.hpp>

#include <Crisp/Core/Application.hpp>
#include <Crisp/Core/Checks.hpp>
#include <Crisp/GUI/ImGuiCameraUtils.hpp>
#include <Crisp/GUI/ImGuiUtils.hpp>
#include <Crisp/IO/FileUtils.hpp>
#include <Crisp/IO/JsonUtils.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPassBuilder.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/RenderPasses/ShadowPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("PbrScene");

constexpr uint32_t kShadowMapSize = 1024;

void setPbrMaterialSceneParams(
    Material& material,
    const ResourceContext& resourceContext,
    const LightSystem& lightSystem,
    const rg::RenderGraph& rg) {
    const auto& imageCache = resourceContext.imageCache;
    const auto& envLight = *lightSystem.getEnvironmentLight();
    material.writeDescriptor(1, 0, *resourceContext.getUniformBuffer("camera"));
    material.writeDescriptor(1, 1, *lightSystem.getCascadedDirectionalLightBuffer());
    material.writeDescriptor(1, 2, envLight.getDiffuseMapView(), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 3, envLight.getSpecularMapView(), imageCache.getSampler("linearMipmap"));
    material.writeDescriptor(1, 4, imageCache.getImageView("brdfLut"), imageCache.getSampler("linearClamp"));
    material.writeDescriptor(1, 5, imageCache.getImageView("sheenLut"), imageCache.getSampler("linearClamp"));
    for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
        for (uint32_t k = 0; k < RendererConfig::VirtualFrameCount; ++k) {
            const auto& shadowMapView{rg.getRenderPass(kCsmPasses[i]).getAttachmentView(0, k)};
            material.writeDescriptor(1, 6, k, i, shadowMapView, &imageCache.getSampler("nearestNeighbor"));
        }
    }
}

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

Material* createPbrMaterial(
    const std::string& materialId,
    const std::string& materialKey,
    ResourceContext& resourceContext,
    const PbrParams& params,
    const TransformBuffer& transformBuffer) {
    auto& imageCache = resourceContext.imageCache;

    auto* material = resourceContext.createMaterial("pbrTex" + materialId, "pbrTex");
    material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

    const auto setMaterialTexture =
        [&material, &imageCache, &materialKey](const uint32_t index, const std::string_view texName) {
            const std::string key = fmt::format("{}-{}", materialKey, texName);
            const std::string fallbackKey = fmt::format("default-{}", texName);
            material->writeDescriptor(
                2, index, imageCache.getImageView(key, fallbackKey), imageCache.getSampler("linearRepeat"));
        };

    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");

    const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
    material->writeDescriptor(
        2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

    return material;
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformBuffer("camera", sizeof(CameraParameters), BufferUpdatePolicy::PerFrame);
    m_renderer->getDebugMarker().setObjectName(m_resourceContext->getUniformBuffer("camera")->get(), "cameraBuffer");

    m_rg = std::make_unique<rg::RenderGraph>();

    addCascadedShadowMapPasses(
        *m_rg, kShadowMapSize, [this](const RenderPassExecutionContext& ctx, const uint32_t cascadeIndex) {
            const uint32_t virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();
            std::vector<DrawCommand> drawCommands{};
            for (const auto& [id, renderNode] : m_renderNodes) {
                createDrawCommand(drawCommands, *renderNode, kCsmPasses[cascadeIndex], virtualFrameIndex);
            }

            const VulkanCommandBuffer commandBuffer(ctx.cmdBuffer);
            for (const auto& drawCommand : drawCommands) {
                RenderGraph::executeDrawCommand(drawCommand, *m_renderer, commandBuffer, virtualFrameIndex);
            }
        });

    addForwardLightingPass(*m_rg, [this](const RenderPassExecutionContext& ctx) {
        const uint32_t virtualFrameIndex = m_renderer->getCurrentVirtualFrameIndex();
        std::vector<DrawCommand> drawCommands{};
        for (const auto& [id, renderNode] : m_renderNodes) {
            createDrawCommand(drawCommands, *renderNode, kForwardLightingPass, virtualFrameIndex);
        }

        createDrawCommand(drawCommands, m_skybox->getRenderNode(), kForwardLightingPass, virtualFrameIndex);

        const VulkanCommandBuffer commandBuffer(ctx.cmdBuffer);
        for (const auto& drawCommand : drawCommands) {
            RenderGraph::executeDrawCommand(drawCommand, *m_renderer, commandBuffer, virtualFrameIndex);
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

    for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
        std::string key = "cascadedShadowMap" + std::to_string(i);
        auto* csmPipeline =
            m_resourceContext->createPipeline(key, "ShadowMap.json", m_rg->getRenderPass(kCsmPasses[i]), 0);
        auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
        csmMaterial->writeDescriptor(0, 0, m_transformBuffer->getDescriptorInfo());
        csmMaterial->writeDescriptor(0, 1, *m_lightSystem->getCascadedDirectionalLightBuffer(i));
    }

    // createPlane();
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

    m_renderer->enqueueResourceUpdate([this](const VkCommandBuffer cmdBuffer) {
        m_rg->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent(), cmdBuffer);
        updateMaterialsWithRenderGraphResources();
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
    m_renderer->enqueueDrawCommand([this](VkCommandBuffer cmdBuffer) { m_rg->execute(cmdBuffer); });
}

void PbrScene::renderGui() {
    drawCameraUi(m_cameraController->getCamera());
    ImGui::Begin("Render Graph");
    drawGui(*m_rg);
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

    if (pbrMaterial.name != "Grass") {
        addPbrTexturesToImageCache(loadPbrTextureGroup(materialPath), pbrMaterial.name, imageCache);
    }
    if (m_shaderBallPbrMaterialKey != "Grass") {
        removePbrTexturesFromImageCache(m_shaderBallPbrMaterialKey, imageCache);
    }
    setMaterialTexture(0, "diffuse");
    setMaterialTexture(1, "metallic");
    setMaterialTexture(2, "roughness");
    setMaterialTexture(3, "normal");
    setMaterialTexture(4, "ao");
    setMaterialTexture(5, "emissive");
    m_renderer->getDevice().flushDescriptorUpdates();

    m_shaderBallPbrMaterialKey = pbrMaterial.name;
}

RenderNode* PbrScene::createRenderNode(std::string id, bool hasTransform) {
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
    addPbrTexturesToImageCache(createDefaultPbrTextureGroup(), "default", imageCache);

    m_resourceContext->createPipeline("pbrTex", "PbrTex.json", m_rg->getRenderPass(kForwardLightingPass), 0);

    setEnvironmentMap("GreenwichPark");
    imageCache.addImageWithView("brdfLut", integrateBrdfLut(m_renderer));
    imageCache.addImageWithView("sheenLut", createSheenLookup(*m_renderer, m_renderer->getResourcesPath()));
}

void PbrScene::setEnvironmentMap(const std::string& envMapName) {
    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / envMapName).unwrap(),
        envMapName);
}

void PbrScene::createSceneObject(const std::filesystem::path& path) {
    // m_cameraController->setTarget(glm::vec3(0.0f, 1.0f, 0.0f));

    if (path.extension() == ".gltf") {
        const std::filesystem::path fullPath{path.is_absolute() ? path : m_renderer->getResourcesPath() / path};
        auto [images, renderObjects] = loadGltfModel(fullPath).unwrap();
        logger->info("Loaded {} objects from {}.", renderObjects.size(), fullPath.generic_string());

        FlatHashSet<int32_t> metallicRoughnessMaps;
        for (const auto& renderObject : renderObjects) {
            if (renderObject.material.textureIndices[2] != -1) {
                metallicRoughnessMaps.insert(renderObject.material.textureIndices[2]);
            }
        }

        for (const auto& [idx, data] : std::views::enumerate(images)) {
            const std::string key = fmt::format("IMAGE-{}", idx);
            if (metallicRoughnessMaps.contains(static_cast<int32_t>(idx))) {
                m_resourceContext->imageCache.addImageWithView(
                    fmt::format("IMAGE-M-{}", idx),
                    createVulkanImage(*m_renderer, data.image.createFromChannel(2), VK_FORMAT_R8G8B8A8_UNORM));
                m_resourceContext->imageCache.addImageWithView(
                    fmt::format("IMAGE-R-{}", idx),
                    createVulkanImage(*m_renderer, data.image.createFromChannel(1), VK_FORMAT_R8G8B8A8_UNORM));
            } else {
                m_resourceContext->imageCache.addImageWithView(
                    key,
                    createVulkanImage(
                        *m_renderer,
                        data.image,
                        data.format == TextureData::Format::sRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM));
            }
        }

        // mesh = std::move(renderObjects.at(0).mesh);

        // material = std::move(renderObjects.at(0).material);
        // material.name = path.stem().generic_string();
        // m_uniformMaterialParams = material.params;

        const auto makeMaterial =
            [](const std::string& materialId,
               const std::array<int32_t, 6>& textureIndices,
               ResourceContext& resourceContext,
               const PbrParams& params,
               const TransformBuffer& transformBuffer) {
                auto& imageCache = resourceContext.imageCache;

                auto* material = resourceContext.createMaterial("pbrTex" + materialId, "pbrTex");
                material->writeDescriptor(0, 0, transformBuffer.getDescriptorInfo());

                const auto setMaterialTexture =
                    [&material,
                     &imageCache](const uint32_t index, const std::string_view texName, const std::string& texKey) {
                        const std::string fallbackKey = fmt::format("default-{}", texName);
                        material->writeDescriptor(
                            2,
                            index,
                            imageCache.getImageView(texKey, fallbackKey),
                            imageCache.getSampler("linearRepeat"));
                    };

                setMaterialTexture(0, "diffuse", fmt::format("IMAGE-{}", textureIndices[0]));
                setMaterialTexture(1, "metallic", fmt::format("IMAGE-M-{}", textureIndices[2]));
                setMaterialTexture(2, "roughness", fmt::format("IMAGE-R-{}", textureIndices[3]));
                setMaterialTexture(3, "normal", fmt::format("IMAGE-{}", textureIndices[1]));
                setMaterialTexture(4, "ao", fmt::format("IMAGE-{}", textureIndices[4]));
                setMaterialTexture(5, "emissive", fmt::format("IMAGE-{}", textureIndices[5]));

                const std::string paramsBufferKey{fmt::format("{}-params", materialId)};
                material->writeDescriptor(
                    2, 6, *resourceContext.createUniformBuffer(paramsBufferKey, params, BufferUpdatePolicy::PerFrame));

                return material;
            };

        for (auto&& [idx, renderObject] : std::views::enumerate(renderObjects)) {
            const std::string materialName = fmt::format("material_{}", idx);
            const std::string entityName = fmt::format("renderObject_{}", idx);
            m_shaderBallPbrMaterialKey = fmt::format("material_{}", idx);
            // addPbrTexturesToImageCache(renderObject.material.textures, materialName, m_resourceContext->imageCache);

            logger->info("Adding object {} with {} triangles.", idx, renderObject.mesh.getTriangleCount());

            m_resourceContext->addGeometry(entityName, createFromMesh(*m_renderer, renderObject.mesh, kPbrVertexFormat));

            auto* sceneObject = createRenderNode(entityName, true);
            sceneObject->geometry = m_resourceContext->getGeometry(entityName);
            sceneObject->transformPack->M = renderObject.transform;
            sceneObject->pass(kForwardLightingPass).material = makeMaterial(
                entityName,
                renderObject.material.textureIndices,
                *m_resourceContext,
                renderObject.material.params,
                *m_transformBuffer);
            setPbrMaterialSceneParams(
                *sceneObject->pass(kForwardLightingPass).material, *m_resourceContext, *m_lightSystem, *m_rg);
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

        material.name = "vokselia";
        material.textures = createDefaultPbrTextureGroup();
        material.textures.albedo =
            loadImage(m_renderer->getResourcesPath() / "Meshes/vokselia_spawn.png", 4, FlipAxis::Y).unwrap();

        const glm::mat4 translation = glm::translate(glm::vec3(0.0f, -mesh.getBoundingBox().min.y, 0.0f));
        const float maxDimLength = mesh.getBoundingBox().getMaximumExtent();
        sceneObject->transformPack->M = translation * glm::scale(glm::vec3(10.0f / maxDimLength));
    }

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
    const std::string entityName{"floor"};
    const TriangleMesh planeMesh{createPlaneMesh(flatten(kPbrVertexFormat), 200.0f)};
    m_resourceContext->addGeometry(entityName, createFromMesh(*m_renderer, planeMesh, kPbrVertexFormat));

    const auto materialPath{m_renderer->getResourcesPath() / "Textures/PbrMaterials" / "Grass"};
    PbrMaterial material{};
    material.name = materialPath.stem().string();
    material.params.uvScale = glm::vec2(100.0f, 100.0f);
    material.textures = loadPbrTextureGroup(materialPath);
    addPbrTexturesToImageCache(material.textures, material.name, m_resourceContext->imageCache);

    auto* floor = createRenderNode(entityName, true);
    floor->transformPack->M = glm::scale(glm::vec3(1.0, 1.0f, 1.0f));
    floor->geometry = m_resourceContext->getGeometry(entityName);
    floor->pass(kForwardLightingPass).material =
        createPbrMaterial(entityName, material.name, *m_resourceContext, material.params, *m_transformBuffer);
    setPbrMaterialSceneParams(*floor->pass(kForwardLightingPass).material, *m_resourceContext, *m_lightSystem, *m_rg);

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

void PbrScene::updateMaterialsWithRenderGraphResources() {
    const auto& imageCache = m_resourceContext->imageCache;
    for (auto&& [name, node] : m_renderNodes) {
        auto& material = node->pass(kForwardLightingPass).material;
        for (uint32_t i = 0; i < kDefaultCascadeCount; ++i) {
            for (uint32_t k = 0; k < RendererConfig::VirtualFrameCount; ++k) {
                const auto& shadowMapView{m_rg->getRenderPass(kCsmPasses[i]).getAttachmentView(0, k)};
                material->writeDescriptor(1, 6, k, i, shadowMapView, &imageCache.getSampler("nearestNeighbor"));
            }
        }
    }
    //*sceneObject->pass(kForwardLightingPass).material, *m_resourceContext,
    //*m_lightSystem, *m_rg
}

void PbrScene::updateSceneViews() {
    const auto& data = m_rg->getBlackboard().get<ForwardLightingData>();
    m_sceneImageViews.resize(RendererConfig::VirtualFrameCount);
    for (auto& sv : m_sceneImageViews) {
        sv = m_rg->createViewFromResource(data.hdrImage);
    }

    m_renderer->setSceneImageViews(m_sceneImageViews);
}

} // namespace crisp