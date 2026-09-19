#include <Crisp/Scenes/MaterialExplorerScene.hpp>

#include <cmath>
#include <numbers>

#include <algorithm>
#include <cstring>
#include <limits>
#include <ranges>
#include <span>

#include <imgui.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Gui/ImGuiUtils.hpp>
#include <Crisp/Image/Io/Utils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Math/Distribution2D.hpp>
#include <Crisp/Mesh/Io/MeshLoader.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>
#include <Crisp/Renderer/VulkanImageUtils.hpp>

namespace crisp {
namespace {

constexpr uint32_t kShadowMapSize{2048};
constexpr uint32_t kMaterialCapacity{6};
constexpr uint32_t kMaterialExplorerSceneIndex{0};
constexpr uint32_t kWhiteFurnaceSceneIndex{1};
constexpr uint8_t kModelVisibilityMask{1u << 0};
constexpr uint8_t kFloorVisibilityMask{1u << 1};
constexpr std::string_view kShaderBallNodeId{"shader-ball"};
constexpr std::string_view kFloorNodeId{"floor"};
constexpr std::string_view kEditableGltfMaterialName{"material_surface"};
constexpr std::string_view kNoMaterialPreset{"(None)"};

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
            commandEncoder.bindDescriptorSets(
                command.material->getDescriptorSetBinding(command.getDynamicBufferOffsets()));
        }
        command.geometry->bindVertexBuffers(commandEncoder, command.firstBuffer, command.bufferCount);
        command.draw(commandEncoder);
    }
}

std::array<const VulkanImageView*, kPbrMapTypeCount> resolvePbrTextureViews(
    const PbrMaterial& material, const ImageCache& imageCache) {
    std::array<const VulkanImageView*, kPbrMapTypeCount> views{};
    for (uint32_t textureIndex = 0; textureIndex < kPbrMapTypeCount; ++textureIndex) {
        views[textureIndex] = &imageCache.getImageView(
            material.textureKeys[textureIndex], fmt::format("default-{}-0", kPbrMapNames[textureIndex]));
    }
    return views;
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
    const auto& features = m_renderer->getDevice().getEnabledFeatures();
    m_rayTracedShadowsSupported = features.rayQuery;
    // The path-traced view addresses its resources through a descriptor heap, so it needs more than a BVH.
    m_pathTracingSupported = features.rayTracing && features.descriptorHeap && features.shaderUntypedPointers;

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
            if (m_useRayTracedShadows || commands.empty() || !m_shaderBallNodes.front()->isVisible) {
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

    if (m_pathTracingSupported) {
        addPathTracedViewPass(*m_renderGraph, [this](const FrameContext& frameContext) {
            if (m_renderMode != RenderMode::Rasterized && m_pathTracedView) {
                m_pathTracedView->trace(frameContext);
            }
        });
    }

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(-glm::normalize(glm::vec3(1.0f, 1.5f, 0.75f)), glm::vec3(0.0f), glm::vec3(-8), glm::vec3(8)),
        kShadowMapSize,
        kDefaultCascadeCount);
    m_lightSystem->setVisualizeCascades(false);
    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaterialCapacity);

    const std::string environmentMapName = args.value("environmentMap", std::string{"NewportLoft"});
    createRenderResources(environmentMapName);

    const std::filesystem::path shaderBallPath = args.value(
        "modelPath", std::string{"glTFSamples/2.0/USDShaderBallForGltf/glTF-Binary/USDShaderBallForGltf.glb"});
    createSceneObjects(shaderBallPath);
    m_showFloor = args.value("showFloor", true);
    m_floorNode->isVisible = m_showFloor;

    // Used for white furnace test.
    if (args.contains("baseColor")) {
        const auto baseColor = args["baseColor"].get<std::vector<float>>();
        CRISP_CHECK_EQ(baseColor.size(), 3);
        m_shaderBallParams.surface.baseColor = glm::vec3(baseColor[0], baseColor[1], baseColor[2]);
    }
    m_shaderBallParams.surface.specularRoughness =
        args.value("specularRoughness", m_shaderBallParams.surface.specularRoughness);
    m_shaderBallParams.surface.baseMetalness = args.value("baseMetalness", m_shaderBallParams.surface.baseMetalness);
    m_pbrMaterialTable->update(m_shaderBallMaterialHandle, m_shaderBallParams);

    createWhiteFurnaceResources();
    createRayTracedShadowResources();
    createPathTracedView();
    if (m_pathTracedView) {
        m_pathTracedView->setVisibilityMask(
            m_showFloor ? kModelVisibilityMask | kFloorVisibilityMask : kModelVisibilityMask);
    }
    rebuildDrawCommands();

    const auto renderMode = args.value("renderMode", std::string{"rasterized"});
    if (renderMode == "path-traced") {
        setRenderMode(RenderMode::PathTraced);
    } else if (renderMode == "white-furnace") {
        setRenderMode(RenderMode::WhiteFurnace);
    }

    m_materialPresetNames.emplace_back(kNoMaterialPreset);
    const auto materialPresetsPath = m_renderer->getResourcesPath() / "Textures/PbrMaterials";
    for (const auto& entry : std::filesystem::directory_iterator(materialPresetsPath)) {
        if (entry.is_directory()) {
            m_materialPresetNames.push_back(entry.path().stem().string());
        }
    }
    std::sort(m_materialPresetNames.begin() + 1, m_materialPresetNames.end());

    m_environmentMapNames.emplace_back(kWhiteFurnaceEnvironmentName);
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
    if (m_pathTracedView) {
        m_pathTracedView->updateDescriptorHeap(*m_renderGraph);
        m_pathTracedView->resetAccumulation();
    }
    updatePresentedImage();
}

void MaterialExplorerScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto cameraParameters = m_cameraController->getCameraParameters();
    m_transformBuffer->update(cameraParameters.V, cameraParameters.P);
    if (m_pathTracedView) {
        m_pathTracedView->updateCamera(cameraParameters);
    }
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

    if (m_renderMode != RenderMode::Rasterized && m_pathTracedView) {
        m_pathTracedView->uploadFrameData(frameContext);
    }

    m_renderGraph->execute(frameContext);
}

void MaterialExplorerScene::drawGui() {
    drawCameraPivot(*m_cameraController);

    ImGui::Begin("Material Explorer");

    if (m_pathTracingSupported) {
        int mode = static_cast<int>(m_renderMode);
        if (ImGui::RadioButton("Rasterized", &mode, static_cast<int>(RenderMode::Rasterized))) {
            setRenderMode(RenderMode::Rasterized);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Path traced", &mode, static_cast<int>(RenderMode::PathTraced))) {
            setRenderMode(RenderMode::PathTraced);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("White furnace", &mode, static_cast<int>(RenderMode::WhiteFurnace))) {
            setRenderMode(RenderMode::WhiteFurnace);
        }
        if (m_renderMode != RenderMode::Rasterized) {
            m_pathTracedView->drawGui(m_renderMode != RenderMode::WhiteFurnace);
        }
        if (m_renderMode == RenderMode::PathTraced) {
            ImGui::TextWrapped(
                "Reference view: material textures and normal maps are enabled; the environment remains the only "
                "light, so it will not match the rasterized view exactly.");
        } else if (m_renderMode == RenderMode::WhiteFurnace) {
            ImGui::TextWrapped(
                "White furnace: the selected material on a unit sphere under constant unit radiance. The floor, "
                "shader-ball geometry, textures, and direct lights are excluded.");
        }
    } else {
        ImGui::TextDisabled("Path tracing unavailable (needs ray tracing and descriptor heaps)");
    }
    ImGui::Separator();

    gui::drawComboBox(
        "Texture Set", m_materialPresetName, m_materialPresetNames, [this](const std::string& selectedItem) {
            setMaterialPreset(selectedItem);
        });

    bool materialChanged = false;
    materialChanged |= ImGui::SliderFloat("Base Weight", &m_shaderBallParams.surface.baseWeight, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::ColorEdit3("Base Color", &m_shaderBallParams.surface.baseColor.x);
    materialChanged |= ImGui::SliderFloat("Geometry Opacity", &m_shaderBallParams.geometryOpacity, 0.0f, 1.0f, "%.3f");
    materialChanged |=
        ImGui::SliderFloat("Base Metalness", &m_shaderBallParams.surface.baseMetalness, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat(
        "Base Diffuse Roughness", &m_shaderBallParams.surface.baseDiffuseRoughness, 0.0f, 1.0f, "%.3f");
    materialChanged |=
        ImGui::SliderFloat("Specular Weight", &m_shaderBallParams.surface.specularWeight, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::ColorEdit3("Specular Color", &m_shaderBallParams.surface.specularColor.x);
    materialChanged |=
        ImGui::SliderFloat("Specular Roughness", &m_shaderBallParams.surface.specularRoughness, 0.001f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Specular IOR", &m_shaderBallParams.surface.specularIor, 1.0f, 3.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Ambient Occlusion", &m_shaderBallParams.aoStrength, 0.0f, 1.0f, "%.3f");
    materialChanged |= ImGui::SliderFloat("Normal Strength", &m_shaderBallParams.normalScale, 0.0f, 2.0f, "%.3f");
    const glm::vec2 previousUvScale = m_shaderBallParams.uvScale;
    if (ImGui::SliderFloat2("UV Scale", &m_shaderBallParams.uvScale.x, 0.1f, 10.0f, "%.2f")) {
        if (ImGui::GetIO().KeyAlt) {
            if (m_shaderBallParams.uvScale.x != previousUvScale.x) {
                m_shaderBallParams.uvScale.y = m_shaderBallParams.uvScale.x;
            } else {
                m_shaderBallParams.uvScale.x = m_shaderBallParams.uvScale.y;
            }
        }
        materialChanged = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Hold Alt while editing to set both UV axes.");
    }
    materialChanged |= ImGui::ColorEdit3("Emission Color", &m_shaderBallParams.surface.emissionColor.x);
    materialChanged |=
        ImGui::SliderFloat("Emission Luminance", &m_shaderBallParams.surface.emissionLuminance, 0.0f, 20.0f, "%.3f");

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
        if (m_whiteFurnaceMaterialHandle) {
            m_pbrMaterialTable->update(*m_whiteFurnaceMaterialHandle, m_shaderBallParams);
        }
        if (m_pathTracedView) {
            m_pathTracedView->resetAccumulation();
        }
    }
    if (m_shaderBallParams.flags != previousFlags) {
        updateShaderBallShadowMaterials();
        rebuildDrawCommands();
    }

    if (ImGui::Button("Reset Material")) {
        resetMaterial();
    }

    ImGui::Separator();
    auto directionalLight = m_lightSystem->getDirectionalLight();
    glm::vec3 directionalRadiance = directionalLight.getRadiance();
    bool directionalLightChanged = ImGui::ColorEdit3(
        "Directional Radiance", &directionalRadiance.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    if (ImGui::Button("Disable Directional Light")) {
        directionalRadiance = glm::vec3(0.0f);
        directionalLightChanged = true;
    }
    if (directionalLightChanged) {
        directionalLight.setRadiance(glm::max(directionalRadiance, glm::vec3(0.0f)));
        m_lightSystem->setDirectionalLight(directionalLight);
    }

    if (m_rayTracedShadowsSupported) {
        if (ImGui::Checkbox("Ray-traced shadows", &m_useRayTracedShadows)) {
            updateForwardDrawParameters();
            rebuildDrawCommands();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip(
                "Compare hard, opaque-geometry BVH visibility rays against cascaded shadow maps with PCF.");
        }
    } else {
        ImGui::TextDisabled("Ray-traced shadows unavailable (ray queries unsupported)");
    }
    if (ImGui::Checkbox("Show Floor", &m_showFloor)) {
        m_floorNode->isVisible = m_showFloor;
        if (m_pathTracedView) {
            m_pathTracedView->setVisibilityMask(
                m_showFloor ? kModelVisibilityMask | kFloorVisibilityMask : kModelVisibilityMask);
        }
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

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
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
        "materialExplorerPbr", "PbrTex.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    auto* pbrDoubleSidedPipeline = m_resourceContext->createPipeline(
        "materialExplorerPbrDoubleSided",
        "PbrTexDoubleSided.json",
        {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});

    setEnvironmentMap(environmentMapName);
    imageCache.addImage("brdfLut", loadBrdfLut(m_renderer));

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
                key, variant.pipelineConfig, {m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[cascadeIndex])});
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
        CRISP_CHECK_LE(sceneData.models.size(), kMaterialCapacity - 2);
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
                model.material.params.surface.baseColor = glm::vec3(0.72f, 0.24f, 0.12f);
                model.material.params.surface.baseMetalness = 0.0f;
                model.material.params.surface.specularRoughness = 0.28f;
                model.material.textureKeys[kPbrOrmMapIndex] = PbrImageKeyCreator{"material-explorer"}.createOrmMapKey(0);
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
        shaderBallMaterial.params.surface.baseColor = glm::vec3(0.72f, 0.24f, 0.12f);
        shaderBallMaterial.params.surface.baseMetalness = 0.0f;
        shaderBallMaterial.params.surface.specularRoughness = 0.28f;
        m_shaderBallMaterialHandle =
            addPbrNode(kShaderBallNodeId, shaderBallMesh, shaderBallMaterial, shaderBallTransform, true);
        m_editableMaterialNode = m_renderNodes.at(std::string{kShaderBallNodeId}).get();
        m_shaderBallNodes.push_back(m_editableMaterialNode);
        m_shaderBallParams = createGpuPbrParams(shaderBallMaterial, m_resourceContext->imageCache);
    }

    const auto floorMesh = createPlaneMesh(12.0f, 12.0f);
    const auto floorMaterialPath = m_renderer->getResourcesPath() / "Textures/PbrMaterials/Grass";
    auto [floorMaterial, floorImages] = loadPbrMaterial(floorMaterialPath);
    floorMaterial.params.surface.baseMetalness = 0.0f;
    floorMaterial.params.surface.specularRoughness = 0.85f;
    floorMaterial.params.uvScale = glm::vec2(8.0f);
    addPbrImageGroupToImageCache(floorImages, m_resourceContext->imageCache);
    addPbrNode(kFloorNodeId, floorMesh, floorMaterial, glm::mat4(1.0f), false);
}

void MaterialExplorerScene::createWhiteFurnaceResources() {
    if (!m_pathTracingSupported) {
        return;
    }

    const auto sphereMesh = createSphereMesh();
    constexpr VkBufferUsageFlags2 kAccelerationStructureUsage =
        VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    auto& geometry = m_resourceContext->addGeometry(
        "white-furnace-sphere", createGeometry(*m_renderer, sphereMesh, kPbrVertexFormat, kAccelerationStructureUsage));
    m_whiteFurnaceMaterialHandle = m_pbrMaterialTable->add(m_shaderBallParams);
    m_pathTracedGeometry.push_back({
        .geometry = &geometry,
        .transform = glm::translate(glm::vec3(0.0f, 1.0f, 0.0f)),
        .materialIndex = m_whiteFurnaceMaterialHandle->index,
        .triangleCount = sphereMesh.getTriangleCount(),
        .sceneIndex = kWhiteFurnaceSceneIndex,
    });
}

void MaterialExplorerScene::createRayTracedShadowResources() {
    if (!m_rayTracedShadowsSupported) {
        return;
    }

    CRISP_CHECK(!m_shadowBlases.empty(), "Ray-traced shadows require at least one shadow-casting mesh.");
    std::vector<VulkanAccelerationStructure*> blases;
    blases.reserve(m_shadowBlases.size());
    for (const auto& blas : m_shadowBlases) {
        blases.push_back(blas.get());
    }

    m_shadowTlas = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);
    m_shadowTlas->setDebugName(m_renderer->getDevice(), "Material Explorer Shadow TLAS");
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

void MaterialExplorerScene::createPathTracedView() {
    if (!m_pathTracingSupported) {
        return;
    }

    m_pathTracedView =
        std::make_unique<PathTracedView>(*m_renderer, m_pathTracedGeometry, m_pbrMaterialTable->getDeviceAddress());
    m_pathTracedView->setEnvironmentDistribution(
        *m_environmentEquirectView, m_environmentDistribution.getCdf(), m_environmentExtent.x, m_environmentExtent.y);
    m_pathTracedView->updateDescriptorHeap(*m_renderGraph);
}

void MaterialExplorerScene::setRenderMode(const RenderMode mode) {
    if (m_renderMode == mode) {
        return;
    }

    // Switching re-points the renderer's scene material at the other view's image, which rewrites a descriptor
    // set that in-flight command buffers still reference. The resize path gets its idle from
    // Renderer::recreateSwapChain; a GUI-driven switch has to ask for one. It is a click, so the stall is free.
    m_renderer->finish();

    m_renderMode = mode;
    if (m_pathTracedView) {
        if (mode == RenderMode::WhiteFurnace) {
            m_environmentIntensityBeforeFurnace = m_pathTracedView->getEnvironmentIntensity();
            m_environmentNameBeforeFurnace = m_lightSystem->getEnvironmentLight()->getName();
            m_pathTracedView->setSceneIndex(kWhiteFurnaceSceneIndex);
            m_pathTracedView->setEnvironmentIntensity(1.0f);
            setEnvironmentMap(kWhiteFurnaceEnvironmentName);
        } else {
            if (mode == RenderMode::PathTraced) {
                m_pathTracedView->setSceneIndex(kMaterialExplorerSceneIndex);
            }
            // Only a furnace entered through the mode is undone here; one picked from the combo box stays.
            if (!m_environmentNameBeforeFurnace.empty()) {
                m_pathTracedView->setEnvironmentIntensity(m_environmentIntensityBeforeFurnace);
                const auto previous = std::exchange(m_environmentNameBeforeFurnace, std::string{});
                setEnvironmentMap(previous);
            }
            m_pathTracedView->resetAccumulation();
        }
    }
    updatePresentedImage();
}

void MaterialExplorerScene::updatePresentedImage() {
    // Both views render into the graph; only the presented image changes. A swipe comparison replaces this with
    // a composite pass reading both.
    const bool pathTraced = m_renderMode != RenderMode::Rasterized && m_pathTracedView;
    m_renderer->setSceneImageView(
        pathTraced ? &getPathTracedViewImage(*m_renderGraph)
                   : &m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());
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
    // Every node feeds the path tracer, including ones that cast no shadow, so the usage bits cannot be gated
    // on castsShadow the way the shadow BLAS below is.
    const bool needsAccelerationStructure = m_rayTracedShadowsSupported || m_pathTracingSupported;
    const VkBufferUsageFlags2 accelerationStructureUsage =
        needsAccelerationStructure
            ? VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            : 0;
    auto& geometry = m_resourceContext->addGeometry(
        nodeId, createGeometry(*m_renderer, mesh, kPbrVertexFormat, accelerationStructureUsage));
    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.transformPack->M = modelMatrix;

    if (castsShadow && m_rayTracedShadowsSupported) {
        m_shadowBlases.push_back(
            std::make_unique<VulkanAccelerationStructure>(
                m_renderer->getDevice(),
                createAccelerationStructureGeometry(geometry, 0),
                mesh.getTriangleCount(),
                modelMatrix));
        m_shadowBlases.back()->setDebugName(m_renderer->getDevice(), fmt::format("Material Explorer {} BLAS", nodeId));
    }

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material =
        (material.params.flags & PbrMaterialDoubleSided) != 0
            ? m_pbrDoubleSidedDrawMaterial.get()
            : m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;

    const auto gpuParams = createGpuPbrParams(material, m_resourceContext->imageCache);
    const auto materialHandle = m_pbrMaterialTable->add(gpuParams);
    m_pbrMaterialHandles.emplace(&node, materialHandle);

    if (m_pathTracingSupported) {
        m_pathTracedGeometry.push_back({
            .geometry = &geometry,
            .transform = modelMatrix,
            .materialIndex = materialHandle.index,
            .triangleCount = mesh.getTriangleCount(),
            .sceneIndex = kMaterialExplorerSceneIndex,
            .visibilityMask = nodeId == kFloorNodeId ? kFloorVisibilityMask : kModelVisibilityMask,
            .materialTextures = resolvePbrTextureViews(material, m_resourceContext->imageCache),
        });
    }
    const PbrDrawFlagFlags drawFlags =
        m_useRayTracedShadows ? PbrDrawFlagFlags{PbrDrawFlag::RayTracedShadows} : PbrDrawFlagFlags{};
    const auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle, drawFlags);
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

void MaterialExplorerScene::updateForwardDrawParameters() {
    const PbrDrawFlagFlags drawFlags =
        m_useRayTracedShadows ? PbrDrawFlagFlags{PbrDrawFlag::RayTracedShadows} : PbrDrawFlagFlags{};
    for (const auto& [node, materialHandle] : m_pbrMaterialHandles) {
        node->pass(kForwardLightingPass)
            .setPushConstants(m_pbrMaterialTable->createDrawParameters(materialHandle, drawFlags));
    }
}

void MaterialExplorerScene::setEnvironmentMap(const std::string& environmentMapName) {
    const bool whiteFurnace = environmentMapName == kWhiteFurnaceEnvironmentName;
    const auto environmentMapPath = m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / environmentMapName;
    CRISP_CHECK(
        whiteFurnace || std::filesystem::is_directory(environmentMapPath),
        "Environment map does not exist: {}",
        environmentMapName);

    // Rebuilding the skybox and reconfiguring the forward material rewrite descriptor sets that in-flight command
    // buffers still reference, so this needs the same idle as a render-mode switch. Harmless during construction.
    m_renderer->finish();

    auto iblData = whiteFurnace ? createWhiteFurnaceIblData() : loadImageBasedLightingData(environmentMapPath).unwrap();

    // Next-event estimation needs the equirectangular map and a CDF over it. Weight each texel by luminance and
    // sin(theta): without the sine the poles, which an equirect massively oversamples, would dominate the
    // distribution and starve the horizon.
    {
        // loadImageBasedLightingData flips Y for the cube-map conversion, but environmentDirectionToUv maps
        // +Y to v = 0. Reusing that copy puts the sky underfoot, so load the source in its own orientation.
        const auto equirect =
            whiteFurnace
                ? createWhiteFurnaceEquirect()
                : loadImage(environmentMapPath / fmt::format("{}.hdr", environmentMapName), 4, FlipAxis::None).unwrap();
        const uint32_t width = equirect.getWidth();
        const uint32_t height = equirect.getHeight();
        const uint32_t channels = equirect.getChannelCount();
        const auto* pixels = reinterpret_cast<const float*>(equirect.getData()); // NOLINT

        std::vector<float> weights(static_cast<size_t>(width) * height);
        for (uint32_t y = 0; y < height; ++y) {
            const float sinTheta =
                std::sin(std::numbers::pi_v<float> * (static_cast<float>(y) + 0.5f) / static_cast<float>(height));
            for (uint32_t x = 0; x < width; ++x) {
                const auto* texel = pixels + (static_cast<size_t>(y) * width + x) * channels;
                const float luminance = 0.2126f * texel[0] + 0.7152f * texel[1] + 0.0722f * texel[2];
                weights[static_cast<size_t>(y) * width + x] = std::max(luminance, 0.0f) * sinTheta;
            }
        }
        m_environmentDistribution = Distribution2D(weights, width, height);
        m_environmentEquirect = createVulkanImage(*m_renderer, equirect, VK_FORMAT_R32G32B32A32_SFLOAT);
        m_environmentEquirectView = createView(m_renderer->getDevice(), *m_environmentEquirect, VK_IMAGE_VIEW_TYPE_2D);
        m_environmentExtent = {width, height};
    }

    m_lightSystem->setEnvironmentMap(std::move(iblData), environmentMapName);
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
    if (m_pathTracedView && m_renderMode != RenderMode::Rasterized) {
        m_pathTracedView->setEnvironmentDistribution(
            *m_environmentEquirectView, m_environmentDistribution.getCdf(), m_environmentExtent.x, m_environmentExtent.y);
    }
}

void MaterialExplorerScene::setMaterialPreset(const std::string& materialPresetName) {
    if (materialPresetName == m_materialPresetName) {
        return;
    }

    PbrMaterial* preset{nullptr};
    PbrMaterial parameterOnlyMaterial{.name = "material-explorer"};
    if (materialPresetName == kNoMaterialPreset) {
        const PbrImageKeyCreator keyCreator{parameterOnlyMaterial.name};
        parameterOnlyMaterial.textureKeys = {
            keyCreator.createAlbedoMapKey(0),
            keyCreator.createNormalMapKey(0),
            keyCreator.createOrmMapKey(0),
            keyCreator.createEmissiveMapKey(0),
        };
        parameterOnlyMaterial.params.surface.baseColor = glm::vec3(0.72f, 0.24f, 0.12f);
        parameterOnlyMaterial.params.surface.specularRoughness = 0.28f;
        preset = &parameterOnlyMaterial;
    } else {
        auto found = m_materialPresets.find(materialPresetName);
        if (found == m_materialPresets.end()) {
            const auto materialPath = m_renderer->getResourcesPath() / "Textures/PbrMaterials" / materialPresetName;
            auto [material, images] = loadPbrMaterial(materialPath);
            addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);
            found = m_materialPresets.emplace(materialPresetName, std::move(material)).first;
        }
        preset = &found->second;
    }

    auto params = createGpuPbrParams(*preset, m_resourceContext->imageCache);

    // Texture sets author these values; the remaining OpenPBR controls stay under the explorer sliders.
    params.surface.baseWeight = m_shaderBallParams.surface.baseWeight;
    params.surface.baseDiffuseRoughness = m_shaderBallParams.surface.baseDiffuseRoughness;
    params.surface.specularColor = m_shaderBallParams.surface.specularColor;
    params.surface.specularWeight = m_shaderBallParams.surface.specularWeight;
    params.surface.specularIor = m_shaderBallParams.surface.specularIor;
    params.uvScale = m_shaderBallParams.uvScale;
    params.geometryOpacity = m_shaderBallParams.geometryOpacity;
    params.alphaCutoff = m_shaderBallParams.alphaCutoff;
    params.flags = m_shaderBallParams.flags;

    m_shaderBallParams = params;
    m_materialPresetName = materialPresetName;
    m_pbrMaterialTable->update(m_shaderBallMaterialHandle, m_shaderBallParams);
    if (m_whiteFurnaceMaterialHandle) {
        m_pbrMaterialTable->update(*m_whiteFurnaceMaterialHandle, m_shaderBallParams);
    }
    if (m_pathTracedView) {
        m_pathTracedView->setMaterialTextures(
            m_shaderBallMaterialHandle.index, resolvePbrTextureViews(*preset, m_resourceContext->imageCache));
    }
}

void MaterialExplorerScene::resetMaterial() {
    const auto samplerIndex = m_shaderBallParams.samplerIndex;
    const auto baseColorTex = m_shaderBallParams.baseColorTex;
    const auto normalTex = m_shaderBallParams.normalTex;
    const auto ormTex = m_shaderBallParams.ormTex;
    const auto emissionTex = m_shaderBallParams.emissionTex;

    m_shaderBallParams = {};
    m_shaderBallParams.surface.baseColor = glm::vec3(0.72f, 0.24f, 0.12f);
    m_shaderBallParams.surface.baseMetalness = 0.0f;
    m_shaderBallParams.surface.specularRoughness = 0.28f;
    m_shaderBallParams.samplerIndex = samplerIndex;
    m_shaderBallParams.baseColorTex = baseColorTex;
    m_shaderBallParams.normalTex = normalTex;
    m_shaderBallParams.ormTex = ormTex;
    m_shaderBallParams.emissionTex = emissionTex;
    m_pbrMaterialTable->update(m_shaderBallMaterialHandle, m_shaderBallParams);
    if (m_whiteFurnaceMaterialHandle) {
        m_pbrMaterialTable->update(*m_whiteFurnaceMaterialHandle, m_shaderBallParams);
    }
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
