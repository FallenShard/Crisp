#include <Crisp/Scenes/MaterialExplorerScene.hpp>

#include <algorithm>
#include <limits>
#include <ranges>
#include <span>

#include <imgui.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>

namespace crisp {
namespace {

constexpr uint32_t kShadowMapSize{2048};
constexpr uint32_t kMaterialCapacity{5};
constexpr std::string_view kShaderBallNodeId{"shader-ball"};
constexpr std::string_view kFloorNodeId{"floor"};
constexpr std::string_view kEditableGltfMaterialName{"material_surface"};

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

const ShadowMaterialVariant& getShadowMaterialVariant(const uint32_t flags) {
    const bool alphaMasked = (flags & PbrMaterialAlphaMask) != 0;
    const bool doubleSided = (flags & PbrMaterialDoubleSided) != 0;
    return kShadowMaterialVariants[static_cast<size_t>(alphaMasked) * 2 + static_cast<size_t>(doubleSided)];
}

std::string createShadowMaterialKey(const uint32_t cascadeIndex, const std::string_view suffix) {
    return fmt::format("materialExplorerShadow{}{}", cascadeIndex, suffix);
}

void appendDrawCommands(
    std::vector<DrawCommand>& commands, const RenderNode& renderNode, const std::string_view renderPass) {
    for (const auto& [key, materialMap] : renderNode.materials) {
        if (key.renderPassName != renderPass) {
            continue;
        }
        for (const auto& [part, material] : materialMap) {
            commands.push_back(material.createDrawCommand(renderNode));
        }
    }
}

void executeDrawCommands(const std::span<const DrawCommand> commands, const VulkanCommandEncoder& commandEncoder) {
    const VulkanPipeline* boundPipeline{nullptr};
    for (const auto& command : commands) {
        if (boundPipeline != command.pipeline) {
            commandEncoder.bindPipeline(*command.pipeline);
            boundPipeline = command.pipeline;
        }
        commandEncoder.setPushConstants(*command.pipeline->getPipelineLayout(), command.pushConstantView.asSpan());
        if (command.material) {
            commandEncoder.bindDescriptorSets(command.material->getDescriptorSetBinding(command.dynamicBufferOffsets));
        }
        command.geometry->bindVertexBuffers(commandEncoder, command.firstBuffer, command.bufferCount);
        command.drawFunc(commandEncoder, command.geometryView);
    }
}

PbrImageGroup createMaterialExplorerImageGroup() {
    auto imageGroup = createDefaultPbrImageGroup();
    imageGroup.name = "material-explorer";
    imageGroup.ormMaps[0] = Image(std::vector<uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4 * sizeof(uint8_t));
    return imageGroup;
}

} // namespace

MaterialExplorerScene::MaterialExplorerScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_cameraController->setTarget(glm::vec3(0.0f, 1.0f, 0.0f));
    m_cameraController->setDistance(4.5f);
    m_cameraController->setOrientation(glm::radians(45.0f), glm::radians(-24.0f));
    m_cameraController->setPanSpeed(2.0f);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    m_renderGraph = std::make_unique<rg::RenderGraph>();
    addCascadedShadowMapPasses(
        *m_renderGraph, kShadowMapSize, [this](const FrameContext& frameContext, const uint32_t cascadeIndex) {
            const auto& commands = m_shadowDrawCommands[cascadeIndex];
            if (commands.empty() || !m_shaderBallNodes.front()->isVisible) {
                return;
            }
            const auto& layout = *commands.front().pipeline->getPipelineLayout();
            m_renderer->getBindlessImageRegistry().bind(
                frameContext.commandEncoder,
                layout.getHandle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                BindlessImageRegistry::kGlobalSetIndex);
            executeDrawCommands(commands, frameContext.commandEncoder);
        });

    addForwardLightingPass(*m_renderGraph, [this](const FrameContext& frameContext) {
        const auto& layout = *m_forwardPassMaterial->getPipeline()->getPipelineLayout();
        m_renderer->getBindlessImageRegistry().bind(
            frameContext.commandEncoder,
            layout.getHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            BindlessImageRegistry::kGlobalSetIndex);
        frameContext.commandEncoder.bindDescriptorSets(m_forwardPassMaterial->getDescriptorSetBinding());

        if (m_shaderBallNodes.front()->isVisible) {
            executeDrawCommands(m_shaderBallForwardDrawCommands, frameContext.commandEncoder);
        }
        if (m_floorNode->isVisible) {
            executeDrawCommands(m_floorForwardDrawCommands, frameContext.commandEncoder);
        }
        executeDrawCommands(std::span<const DrawCommand>(&m_skyboxDrawCommand, 1), frameContext.commandEncoder);
    });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::normalize(glm::vec3(1.0f, 1.5f, 0.75f)), glm::vec3(3.0f), glm::vec3(-8), glm::vec3(8)),
        kShadowMapSize,
        kDefaultCascadeCount);
    m_lightSystem->setVisualizeCascades(false);
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaterialCapacity);

    const std::string environmentMapName = args.value("environmentMap", std::string{"NewportLoft"});
    createRenderResources(environmentMapName);

    const std::filesystem::path shaderBallPath = args.value(
        "modelPath", std::string{"glTFSamples/2.0/USDShaderBallForGltf/glTF-Binary/USDShaderBallForGltf.glb"});
    createSceneObjects(shaderBallPath);
    rebuildDrawCommands();

    const auto environmentMapsPath = m_renderer->getResourcesPath() / "Textures/EnvironmentMaps";
    for (const auto& entry : std::filesystem::directory_iterator(environmentMapsPath)) {
        if (entry.is_directory()) {
            m_environmentMapNames.push_back(entry.path().stem().string());
        }
    }
    std::ranges::sort(m_environmentMapNames);
}

void MaterialExplorerScene::resize(const int width, const int height) {
    m_cameraController->onViewportResized(width, height);
    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());
}

void MaterialExplorerScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto cameraParameters = m_cameraController->getCameraParameters();
    m_transformBuffer->update(cameraParameters.V, cameraParameters.P);
}

void MaterialExplorerScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("MaterialExplorerScene::render", frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        (kVertexUniformRead | kFragmentUniformRead | kFragmentRead) >> kTransferWrite);

    const auto cameraParameters = m_cameraController->getCameraParameters();
    m_lightSystem->update(m_cameraController->getCamera(), frameContext.virtualFrameIndex);
    m_lightSystem->getCascadedDirectionalLightBuffer()->updateDeviceBuffer(frameContext.commandEncoder);

    m_skybox->updateTransforms(cameraParameters.V, cameraParameters.P, frameContext.virtualFrameIndex);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);

    auto* cameraBuffer = m_resourceContext->getRingBuffer("camera");
    cameraBuffer->updateStagingBufferFromStruct(cameraParameters, frameContext.virtualFrameIndex);
    cameraBuffer->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->updateStagingBuffer(frameContext.virtualFrameIndex);
    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
    m_pbrMaterialTable->updateDeviceBuffer(*frameContext.stagingBelt, frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead | kFragmentRead));
    m_renderGraph->execute(frameContext);
}

void MaterialExplorerScene::drawGui() {
    drawCameraPivot(*m_cameraController);

    ImGui::Begin("Material Explorer");

    bool materialChanged = false;
    materialChanged |= ImGui::ColorEdit3("Base Color", &m_shaderBallParams.albedo.x);
    materialChanged |= ImGui::SliderFloat("Opacity", &m_shaderBallParams.albedo.a, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Metallic", &m_shaderBallParams.metallic, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Roughness", &m_shaderBallParams.roughness, 0.001f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Ambient Occlusion", &m_shaderBallParams.aoStrength, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Normal Strength", &m_shaderBallParams.normalScale, 0.0f, 2.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat2("UV Scale", &m_shaderBallParams.uvScale.x, 0.1f, 10.0f, "%.2f");
    materialChanged |= ImGui::ColorEdit3(
        "Emissive", &m_shaderBallParams.emissiveFactor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);

    const uint32_t previousFlags = m_shaderBallParams.flags;
    bool alphaMasked = (m_shaderBallParams.flags & PbrMaterialAlphaMask) != 0;
    bool doubleSided = (m_shaderBallParams.flags & PbrMaterialDoubleSided) != 0;
    if (ImGui::Checkbox("Alpha Mask", &alphaMasked)) {
        m_shaderBallParams.flags ^= PbrMaterialAlphaMask;
        materialChanged = true;
    }
    if (alphaMasked) {
        materialChanged |= ImGui::SliderFloat("Alpha Cutoff", &m_shaderBallParams.alphaCutoff, 0.0f, 1.0f, "%.3f");
    }
    if (ImGui::Checkbox("Double Sided", &doubleSided)) {
        m_shaderBallParams.flags ^= PbrMaterialDoubleSided;
        materialChanged = true;
    }

    if (materialChanged) {
        m_pbrMaterialTable->update(m_shaderBallMaterialHandle, m_shaderBallParams);
    }
    if (m_shaderBallParams.flags != previousFlags) {
        updateShaderBallShadowMaterials();
        rebuildDrawCommands();
    }

    if (ImGui::Button("Reset Material")) {
        resetMaterial();
    }

    ImGui::Separator();
    if (ImGui::Checkbox("Show Floor", &m_showFloor)) {
        m_floorNode->isVisible = m_showFloor;
    }
    gui::drawComboBox(
        "Environment",
        m_lightSystem->getEnvironmentLight()->getName(),
        m_environmentMapNames,
        [this](const std::string& selectedItem) { setEnvironmentMap(selectedItem); });

    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraControllerUi(*m_cameraController, false);
        drawCameraUi(m_cameraController->getCamera(), false);
        if (ImGui::Button("Reset Camera")) {
            m_cameraController->setTarget(glm::vec3(0.0f, 1.0f, 0.0f));
            m_cameraController->setDistance(4.5f);
            m_cameraController->setOrientation(glm::radians(45.0f), glm::radians(-24.0f));
        }
    }
    ImGui::End();
}

void MaterialExplorerScene::createRenderResources(const std::string& environmentMapName) {
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    addPbrImageGroupToImageCache(createDefaultPbrImageGroup(), imageCache);
    addPbrImageGroupToImageCache(createMaterialExplorerImageGroup(), imageCache);

    auto* pbrPipeline = m_resourceContext->createPipeline(
        "materialExplorerPbr", "PbrTex.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    auto* pbrDoubleSidedPipeline = m_resourceContext->createPipeline(
        "materialExplorerPbrDoubleSided",
        "PbrTexDoubleSided.json",
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));

    setEnvironmentMap(environmentMapName);
    imageCache.addImage("brdfLut", integrateBrdfLut(m_renderer));

    m_forwardPassMaterial = std::make_unique<Material>(
        pbrPipeline, pbrPipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 1, 1);
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);

    m_pbrMaterialTable = std::make_unique<PbrMaterialTable>(m_renderer->getDevice(), kMaterialCapacity);
    m_pbrDrawMaterial = std::make_unique<Material>(
        pbrPipeline, pbrPipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 2, 1);
    m_pbrDrawMaterial->writeDescriptor(2, 0, m_transformBuffer->getDescriptorInfo());
    m_pbrDoubleSidedDrawMaterial = std::make_unique<Material>(
        pbrDoubleSidedPipeline, pbrDoubleSidedPipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 2, 1);
    m_pbrDoubleSidedDrawMaterial->writeDescriptor(2, 0, m_transformBuffer->getDescriptorInfo());

    for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
        for (const auto& variant : kShadowMaterialVariants) {
            const auto key = createShadowMaterialKey(cascadeIndex, variant.suffix);
            auto* pipeline = m_resourceContext->createPipeline(
                key, variant.pipelineConfig, m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[cascadeIndex]));
            auto* material = m_resourceContext->createMaterial(key, pipeline);
            material->writeDescriptor(1, 0, m_transformBuffer->getDescriptorInfo());
            material->writeDescriptor(1, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(cascadeIndex));
        }
    }
}

void MaterialExplorerScene::createSceneObjects(const std::filesystem::path& shaderBallPath) {
    const auto absoluteShaderBallPath =
        shaderBallPath.is_absolute() ? shaderBallPath : m_renderer->getResourcesPath() / shaderBallPath;
    const auto extension = absoluteShaderBallPath.extension().string();
    if (extension == ".gltf" || extension == ".glb") {
        auto sceneData = loadGltfAsset(absoluteShaderBallPath).unwrap();
        CRISP_CHECK_LE(sceneData.models.size(), kMaterialCapacity - 1);
        addPbrImageGroupToImageCache(sceneData.images, m_resourceContext->imageCache);

        glm::vec3 boundsMin{std::numeric_limits<float>::max()};
        glm::vec3 boundsMax{std::numeric_limits<float>::lowest()};
        for (const auto& model : sceneData.models) {
            for (const auto& position : model.mesh.getPositions()) {
                const glm::vec3 transformedPosition{model.transform * glm::vec4(position, 1.0f)};
                boundsMin = glm::min(boundsMin, transformedPosition);
                boundsMax = glm::max(boundsMax, transformedPosition);
            }
        }
        const glm::vec3 size = boundsMax - boundsMin;
        const float scale = 2.0f / std::max({size.x, size.y, size.z});
        const glm::vec3 centerXZ((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
        const glm::mat4 normalizationTransform = glm::scale(glm::vec3(scale)) * glm::translate(-centerXZ);

        for (auto&& [modelIndex, model] : std::views::enumerate(sceneData.models)) {
            const bool isEditableMaterial = model.material.name == kEditableGltfMaterialName;
            if (isEditableMaterial) {
                model.material.params.albedo = glm::vec4(0.72f, 0.24f, 0.12f, 1.0f);
                model.material.params.metallic = 0.0f;
                model.material.params.roughness = 0.28f;
            }

            const auto nodeId = fmt::format("{}-{}", kShaderBallNodeId, modelIndex);
            const auto materialHandle =
                addPbrNode(nodeId, model.mesh, model.material, normalizationTransform * model.transform, true);
            auto* node = m_renderNodes.at(nodeId).get();
            m_shaderBallNodes.push_back(node);
            if (isEditableMaterial) {
                CRISP_CHECK(m_editableMaterialNode == nullptr, "The shader ball has multiple editable surfaces.");
                m_editableMaterialNode = node;
                m_shaderBallMaterialHandle = materialHandle;
                m_shaderBallParams = createGpuPbrParams(model.material, m_resourceContext->imageCache);
            }
        }
        CRISP_CHECK(
            m_editableMaterialNode != nullptr, "The GLTF shader ball has no '{}' material.", kEditableGltfMaterialName);
    } else {
        const auto shaderBallMesh = loadTriangleMesh(absoluteShaderBallPath).unwrap();
        const auto& bounds = shaderBallMesh.getBoundingBox();
        const glm::vec3 size = bounds.max - bounds.min;
        const float scale = 2.0f / std::max({size.x, size.y, size.z});
        const glm::vec3 centerXZ(
            (bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y, (bounds.min.z + bounds.max.z) * 0.5f);
        const glm::mat4 shaderBallTransform = glm::scale(glm::vec3(scale)) * glm::translate(-centerXZ);

        PbrMaterial shaderBallMaterial{.name = "shader-ball"};
        const auto keyCreator = PbrImageKeyCreator{"material-explorer"};
        shaderBallMaterial.textureKeys = {
            keyCreator.createAlbedoMapKey(0),
            keyCreator.createNormalMapKey(0),
            keyCreator.createOrmMapKey(0),
            keyCreator.createEmissiveMapKey(0),
        };
        shaderBallMaterial.params.albedo = glm::vec4(0.72f, 0.24f, 0.12f, 1.0f);
        shaderBallMaterial.params.metallic = 0.0f;
        shaderBallMaterial.params.roughness = 0.28f;
        m_shaderBallMaterialHandle =
            addPbrNode(kShaderBallNodeId, shaderBallMesh, shaderBallMaterial, shaderBallTransform, true);
        m_editableMaterialNode = m_renderNodes.at(std::string{kShaderBallNodeId}).get();
        m_shaderBallNodes.push_back(m_editableMaterialNode);
        m_shaderBallParams = createGpuPbrParams(shaderBallMaterial, m_resourceContext->imageCache);
    }

    const auto floorMesh = createPlaneMesh(12.0f, 12.0f);
    const auto floorMaterialPath = m_renderer->getResourcesPath() / "Textures/PbrMaterials/Grass";
    auto [floorMaterial, floorImages] = loadPbrMaterial(floorMaterialPath);
    floorMaterial.params.metallic = 0.0f;
    floorMaterial.params.roughness = 0.85f;
    floorMaterial.params.uvScale = glm::vec2(8.0f);
    addPbrImageGroupToImageCache(floorImages, m_resourceContext->imageCache);
    addPbrNode(kFloorNodeId, floorMesh, floorMaterial, glm::mat4(1.0f), false);
}

RenderNode& MaterialExplorerScene::createRenderNode(const std::string_view nodeId) {
    const auto transformHandle = m_transformBuffer->getNextIndex();
    return *m_renderNodes.emplace(nodeId, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle)).first->second;
}

PbrMaterialHandle MaterialExplorerScene::addPbrNode(
    const std::string_view nodeId,
    const TriangleMesh& mesh,
    const PbrMaterial& material,
    const glm::mat4& modelMatrix,
    const bool castsShadow) {
    auto& geometry = m_resourceContext->addGeometry(nodeId, createGeometry(*m_renderer, mesh, kPbrVertexFormat));
    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.transformPack->M = modelMatrix;

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material =
        (material.params.flags & PbrMaterialDoubleSided) != 0
            ? m_pbrDoubleSidedDrawMaterial.get()
            : m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;

    const auto gpuParams = createGpuPbrParams(material, m_resourceContext->imageCache);
    const auto materialHandle = m_pbrMaterialTable->add(gpuParams);
    const auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle);
    forwardPass.setPushConstants(drawParameters);

    if (castsShadow) {
        const auto& variant = getShadowMaterialVariant(gpuParams.flags);
        const bool alphaMasked = (gpuParams.flags & PbrMaterialAlphaMask) != 0;
        for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
            auto& shadowPass = node.pass(kCsmPasses[cascadeIndex]);
            shadowPass.setGeometry(&geometry, 0, alphaMasked ? 2 : 1);
            shadowPass.material = m_resourceContext->getMaterial(createShadowMaterialKey(cascadeIndex, variant.suffix));
            shadowPass.setPushConstants(drawParameters);
        }
    }

    if (nodeId == kFloorNodeId) {
        m_floorNode = &node;
    }
    return materialHandle;
}

void MaterialExplorerScene::setEnvironmentMap(const std::string& environmentMapName) {
    const auto environmentMapPath = m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / environmentMapName;
    CRISP_CHECK(
        std::filesystem::is_directory(environmentMapPath), "Environment map does not exist: {}", environmentMapName);

    m_lightSystem->setEnvironmentMap(loadImageBasedLightingData(environmentMapPath).unwrap(), environmentMapName);
    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_lightSystem->getEnvironmentLight()->getCubeMapView(),
        m_resourceContext->imageCache.getSampler("linearClamp"));

    std::vector<DrawCommand> skyboxCommands;
    appendDrawCommands(skyboxCommands, m_skybox->getRenderNode(), kForwardLightingPass);
    CRISP_CHECK_EQ(skyboxCommands.size(), 1);
    m_skyboxDrawCommand = std::move(skyboxCommands.front());

    if (m_forwardPassMaterial) {
        configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    }
}

void MaterialExplorerScene::resetMaterial() {
    const auto samplerIndex = m_shaderBallParams.samplerIndex;
    const auto albedoTex = m_shaderBallParams.albedoTex;
    const auto normalTex = m_shaderBallParams.normalTex;
    const auto ormTex = m_shaderBallParams.ormTex;
    const auto emissiveTex = m_shaderBallParams.emissiveTex;

    m_shaderBallParams = {};
    m_shaderBallParams.albedo = glm::vec4(0.72f, 0.24f, 0.12f, 1.0f);
    m_shaderBallParams.metallic = 0.0f;
    m_shaderBallParams.roughness = 0.28f;
    m_shaderBallParams.samplerIndex = samplerIndex;
    m_shaderBallParams.albedoTex = albedoTex;
    m_shaderBallParams.normalTex = normalTex;
    m_shaderBallParams.ormTex = ormTex;
    m_shaderBallParams.emissiveTex = emissiveTex;
    m_pbrMaterialTable->update(m_shaderBallMaterialHandle, m_shaderBallParams);
    updateShaderBallShadowMaterials();
    rebuildDrawCommands();
}

void MaterialExplorerScene::updateShaderBallShadowMaterials() {
    const auto& variant = getShadowMaterialVariant(m_shaderBallParams.flags);
    const bool alphaMasked = (m_shaderBallParams.flags & PbrMaterialAlphaMask) != 0;
    m_editableMaterialNode->pass(kForwardLightingPass).material =
        (m_shaderBallParams.flags & PbrMaterialDoubleSided) != 0
            ? m_pbrDoubleSidedDrawMaterial.get()
            : m_pbrDrawMaterial.get();
    for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
        auto& shadowPass = m_editableMaterialNode->pass(kCsmPasses[cascadeIndex]);
        shadowPass.setGeometry(m_editableMaterialNode->geometry, 0, alphaMasked ? 2 : 1);
        shadowPass.material = m_resourceContext->getMaterial(createShadowMaterialKey(cascadeIndex, variant.suffix));
    }
}

void MaterialExplorerScene::rebuildDrawCommands() {
    for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
        auto& commands = m_shadowDrawCommands[cascadeIndex];
        commands.clear();
        for (const auto* node : m_shaderBallNodes) {
            appendDrawCommands(commands, *node, kCsmPasses[cascadeIndex]);
        }
    }

    m_shaderBallForwardDrawCommands.clear();
    for (const auto* node : m_shaderBallNodes) {
        appendDrawCommands(m_shaderBallForwardDrawCommands, *node, kForwardLightingPass);
    }
    CRISP_CHECK_EQ(m_shaderBallForwardDrawCommands.size(), m_shaderBallNodes.size());

    m_floorForwardDrawCommands.clear();
    appendDrawCommands(m_floorForwardDrawCommands, *m_floorNode, kForwardLightingPass);
    CRISP_CHECK_EQ(m_floorForwardDrawCommands.size(), 1);

    std::vector<DrawCommand> skyboxCommands;
    appendDrawCommands(skyboxCommands, m_skybox->getRenderNode(), kForwardLightingPass);
    CRISP_CHECK_EQ(skyboxCommands.size(), 1);
    m_skyboxDrawCommand = std::move(skyboxCommands.front());
}

void MaterialExplorerScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](const Key key, int) {
        if (key == Key::F5) {
            m_resourceContext->recreatePipelines();
            rebuildDrawCommands();
        }
    }));
}

} // namespace crisp
