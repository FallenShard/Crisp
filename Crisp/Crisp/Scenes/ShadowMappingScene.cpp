#include <Crisp/Scenes/ShadowMappingScene.hpp>

#include <cmath>
#include <span>

#include <imgui.h>

#include <Crisp/Gui/ImGuiCameraUtils.hpp>
#include <Crisp/Lights/EnvironmentLightIo.hpp>
#include <Crisp/Materials/PbrMaterialUtils.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>
#include <Crisp/Models/Skybox.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>
#include <Crisp/Renderer/RenderPasses/ForwardLightingPass.hpp>

namespace crisp {
namespace {

constexpr uint32_t kShadowMapSize{2048};

std::string createShadowMaterialKey(const uint32_t cascadeIndex) {
    return fmt::format("shadowMappingCascade{}", cascadeIndex);
}

BoundingBox3 transformBoundingBox(const BoundingBox3& localBounds, const glm::mat4& transform) {
    BoundingBox3 worldBounds;
    for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
        worldBounds.expandBy(glm::vec3(transform * glm::vec4(localBounds.getCorner(cornerIndex), 1.0f)));
    }
    return worldBounds;
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
        if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Viewport) &&
            command.viewport.width != 0.0f) {
            commandEncoder.setViewport(command.viewport);
        }
        if (command.pipeline->getDynamicStateFlags().contains(PipelineDynamicState::Scissor) &&
            command.scissor.extent.width != 0) {
            commandEncoder.setScissor(command.scissor);
        }
        commandEncoder.setPushConstants(*command.pipeline->getPipelineLayout(), command.pushConstantView.asSpan());
        if (command.material != nullptr) {
            commandEncoder.bindDescriptorSets(command.material->getDescriptorSetBinding(command.dynamicBufferOffsets));
        }
        command.geometry->bindVertexBuffers(commandEncoder, command.firstBuffer, command.bufferCount);
        command.drawFunc(commandEncoder, command.geometryView);
    }
}

} // namespace

ShadowMappingScene::ShadowMappingScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    m_cameraController->setTarget(glm::vec3(0.0f, 2.0f, -4.0f));
    m_cameraController->setDistance(18.0f);
    m_cameraController->setOrientation(glm::radians(35.0f), glm::radians(-22.0f));
    m_cameraController->setPanSpeed(5.0f);
    m_resourceContext->createUniformRingBuffer("camera", sizeof(CameraParameters));

    m_renderGraph = std::make_unique<rg::RenderGraph>();
    addCascadedShadowMapPasses(
        *m_renderGraph, kShadowMapSize, [this](const FrameContext& frameContext, const uint32_t cascadeIndex) {
            const auto& commands = m_shadowDrawCommands[cascadeIndex];
            if (commands.empty()) {
                return;
            }

            const auto& layout = *commands.front().command.pipeline->getPipelineLayout();
            m_renderer->getBindlessImageRegistry().bind(
                frameContext.commandEncoder,
                layout.getHandle(),
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                BindlessImageRegistry::kGlobalSetIndex);

            for (const auto& cached : commands) {
                if (!cached.renderNode->isVisible ||
                    !m_lightSystem->isCascadeCasterVisible(cascadeIndex, cached.worldBounds)) {
                    continue;
                }
                executeDrawCommands(std::span<const DrawCommand>(&cached.command, 1), frameContext.commandEncoder);
            }
        });

    addForwardLightingPass(*m_renderGraph, [this](const FrameContext& frameContext) {
        const auto& layout = *m_forwardPassMaterial->getPipeline()->getPipelineLayout();
        m_renderer->getBindlessImageRegistry().bind(
            frameContext.commandEncoder,
            layout.getHandle(),
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            BindlessImageRegistry::kGlobalSetIndex);
        frameContext.commandEncoder.bindDescriptorSets(m_forwardPassMaterial->getDescriptorSetBinding());

        executeDrawCommands(m_forwardDrawCommands, frameContext.commandEncoder);
        executeDrawCommands(std::span<const DrawCommand>(&m_skyboxDrawCommand, 1), frameContext.commandEncoder);
    });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());

    m_splitLambda = args.value("splitLambda", m_splitLambda);
    m_cascadeBlendFraction = args.value("cascadeBlendFraction", m_cascadeBlendFraction);
    m_casterDepthExtrusion = args.value("casterDepthExtrusion", m_casterDepthExtrusion);
    m_visualizeCascades = args.value("visualizeCascades", m_visualizeCascades);

    m_lightSystem = std::make_unique<LightSystem>(
        m_renderer,
        DirectionalLight(glm::normalize(m_lightDirection), glm::vec3(3.0f), glm::vec3(-30.0f), glm::vec3(30.0f)),
        kShadowMapSize,
        kDefaultCascadeCount);
    m_lightSystem->setSplitLambda(m_splitLambda);
    m_lightSystem->setCascadeBlendFraction(m_cascadeBlendFraction);
    m_lightSystem->setCasterDepthExtrusion(m_casterDepthExtrusion);
    m_lightSystem->setVisualizeCascades(m_visualizeCascades);

    m_transformBuffer = std::make_unique<TransformBuffer>(m_renderer, kMaximumObjectCount);
    createRenderResources(args.value("environmentMap", std::string{"GreenwichPark"}));
    createShowcase();
    rebuildDrawCommands();
}

void ShadowMappingScene::resize(const int width, const int height) {
    m_cameraController->onViewportResized(width, height);
    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&ForwardLightingPassData::hdrImage>());
}

void ShadowMappingScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto cameraParameters = m_cameraController->getCameraParameters();
    m_transformBuffer->update(cameraParameters.V, cameraParameters.P);

    if (m_animateLight) {
        const float azimuth = updateParams.totalTimeSec * 0.25f;
        m_lightDirection = glm::normalize(glm::vec3(std::cos(azimuth), -1.5f, std::sin(azimuth)));
        auto light = m_lightSystem->getDirectionalLight();
        light.setDirection(m_lightDirection);
        m_lightSystem->setDirectionalLight(light);
    }
}

void ShadowMappingScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("ShadowMappingScene::render", frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        (kVertexUniformRead | kFragmentUniformRead | kFragmentRead) >> kTransferWrite);

    const auto cameraParameters = m_cameraController->getCameraParameters();
    m_lightSystem->update(m_cameraController->getCamera(), frameContext.virtualFrameIndex);
    m_lightSystem->getCascadedDirectionalLightBuffer()->updateDeviceBuffer(frameContext.commandEncoder);

    m_skybox->updateTransforms(cameraParameters.V, cameraParameters.P, frameContext.virtualFrameIndex);
    m_skybox->updateDeviceBuffer(frameContext.commandEncoder);

    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(
        cameraParameters, frameContext.virtualFrameIndex);
    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder);

    m_transformBuffer->updateStagingBuffer(frameContext.virtualFrameIndex);
    m_transformBuffer->getUniformBuffer()->updateDeviceBuffer(frameContext.commandEncoder);
    m_pbrMaterialTable->updateDeviceBuffer(*frameContext.stagingBelt, frameContext.commandEncoder);

    frameContext.commandEncoder.insertBarrier(
        kTransferWrite >> (kVertexUniformRead | kFragmentUniformRead | kFragmentRead));
    m_renderGraph->execute(frameContext);
}

void ShadowMappingScene::drawGui() {
    drawCameraPivot(*m_cameraController);

    ImGui::Begin("Shadow Mapping");
    if (ImGui::CollapsingHeader("Cascades", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("Split lambda", &m_splitLambda, 0.0f, 1.0f, "%.3f")) {
            m_lightSystem->setSplitLambda(m_splitLambda);
        }
        if (ImGui::SliderFloat("Blend fraction", &m_cascadeBlendFraction, 0.0f, 0.3f, "%.3f")) {
            m_lightSystem->setCascadeBlendFraction(m_cascadeBlendFraction);
        }
        if (ImGui::SliderFloat("Caster extrusion", &m_casterDepthExtrusion, 0.0f, 200.0f, "%.1f")) {
            m_lightSystem->setCasterDepthExtrusion(m_casterDepthExtrusion);
        }
        if (ImGui::Checkbox("Visualize cascades", &m_visualizeCascades)) {
            m_lightSystem->setVisualizeCascades(m_visualizeCascades);
        }
    }
    if (ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Animate", &m_animateLight);
        glm::vec3 direction = m_lightDirection;
        if (!m_animateLight && ImGui::SliderFloat3("Direction", &direction.x, -1.0f, 1.0f) &&
            glm::length2(direction) > 0.0f) {
            m_lightDirection = glm::normalize(direction);
            auto light = m_lightSystem->getDirectionalLight();
            light.setDirection(m_lightDirection);
            m_lightSystem->setDirectionalLight(light);
        }
    }
    if (ImGui::CollapsingHeader("Camera")) {
        drawCameraControllerUi(*m_cameraController, false);
        drawCameraUi(m_cameraController->getCamera(), false);
    }
    ImGui::End();

    ImGui::Begin("Render Graph");
    drawRenderGraphGui(*m_renderGraph);
    ImGui::End();
}

RenderNode& ShadowMappingScene::createRenderNode(const std::string_view nodeId) {
    const auto transformHandle = m_transformBuffer->getNextIndex();
    return *m_renderNodes.emplace(nodeId, std::make_unique<RenderNode>(*m_transformBuffer, transformHandle)).first->second;
}

void ShadowMappingScene::createRenderResources(const std::string& environmentMapName) {
    constexpr float kAnisotropy{16.0f};
    constexpr float kMaxLod{9.0f};
    auto& imageCache = m_resourceContext->imageCache;
    imageCache.addSampler("nearestNeighbor", createNearestClampSampler(m_renderer->getDevice()));
    imageCache.addSampler("linearRepeat", createLinearRepeatSampler(m_renderer->getDevice(), kAnisotropy));
    imageCache.addSampler("linearMipmap", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy, kMaxLod));
    imageCache.addSampler("linearClamp", createLinearClampSampler(m_renderer->getDevice(), kAnisotropy));
    addPbrImageGroupToImageCache(createDefaultPbrImageGroup(), imageCache);

    m_lightSystem->setEnvironmentMap(
        loadImageBasedLightingData(m_renderer->getResourcesPath() / "Textures/EnvironmentMaps" / environmentMapName)
            .unwrap(),
        environmentMapName);
    imageCache.addImage("brdfLut", integrateBrdfLut(m_renderer));

    auto* pipeline = m_resourceContext->createPipeline(
        "shadowMappingPbr", "PbrTex.json", m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass));
    m_forwardPassMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 1, 1);
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);

    m_pbrMaterialTable = std::make_unique<PbrMaterialTable>(m_renderer->getDevice(), kMaximumObjectCount);
    m_pbrDrawMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 2, 1);
    m_pbrDrawMaterial->writeDescriptor(2, 0, m_transformBuffer->getDescriptorInfo());

    for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
        const auto key = createShadowMaterialKey(cascadeIndex);
        auto* shadowPipeline = m_resourceContext->createPipeline(
            key, "PbrShadowMap.json", m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[cascadeIndex]));
        auto* shadowMaterial = m_resourceContext->createMaterial(key, shadowPipeline);
        shadowMaterial->writeDescriptor(1, 0, m_transformBuffer->getDescriptorInfo());
        shadowMaterial->writeDescriptor(1, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(cascadeIndex));
    }

    m_skybox = std::make_unique<Skybox>(
        m_renderer,
        m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass),
        m_lightSystem->getEnvironmentLight()->getCubeMapView(),
        imageCache.getSampler("linearClamp"));
}

void ShadowMappingScene::createShowcase() {
    const auto floorMesh = createPlaneMesh(50.0f, 50.0f);
    const auto cubeMesh = createCubeMesh();
    const auto sphereMesh = createSphereMesh();

    const bool rayQueryEnabled = m_renderer->getDevice().getEnabledFeatures().rayQuery;
    const VkBufferUsageFlags2 accelerationStructureUsage =
        rayQueryEnabled
            ? VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_2_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            : 0;

    auto& floorGeometry =
        m_resourceContext->addGeometry("shadow-floor", createGeometry(*m_renderer, floorMesh, kPbrVertexFormat));
    auto& cubeGeometry = m_resourceContext->addGeometry(
        "shadow-cube", createGeometry(*m_renderer, cubeMesh, kPbrVertexFormat, accelerationStructureUsage));
    auto& sphereGeometry =
        m_resourceContext->addGeometry("shadow-sphere", createGeometry(*m_renderer, sphereMesh, kPbrVertexFormat));

    if (rayQueryEnabled) {
        // The shared PBR shader statically declares its optional ray-query descriptor, so Vulkan requires a valid
        // binding even though every draw in this scene selects cascaded shadows.
        m_descriptorFallbackBlas = std::make_unique<VulkanAccelerationStructure>(
            m_renderer->getDevice(),
            createAccelerationStructureGeometry(cubeGeometry, 0),
            cubeMesh.getTriangleCount(),
            glm::mat4(1.0f));
        std::vector<VulkanAccelerationStructure*> blases{m_descriptorFallbackBlas.get()};
        m_descriptorFallbackTlas = std::make_unique<VulkanAccelerationStructure>(m_renderer->getDevice(), blases);
        m_forwardPassMaterial->writeDescriptor(1, 6, m_descriptorFallbackTlas->getDescriptorInfo());

        m_renderer->enqueueResourceUpdate([this](const VulkanCommandEncoder& encoder) {
            encoder.buildAccelerationStructure(*m_descriptorFallbackBlas);
            encoder.insertBarrier(kAccelerationStructureWrite >> kAccelerationStructureRead);
            encoder.buildAccelerationStructure(*m_descriptorFallbackTlas);
            encoder.insertBarrier(kAccelerationStructureWrite >> kFragmentAccelerationStructureRead);
        });
    }

    PbrMaterialParams floorMaterial;
    floorMaterial.baseColor = glm::vec3(0.32f, 0.38f, 0.25f);
    floorMaterial.specularRoughness = 0.82f;
    addObject("floor", floorGeometry, floorMesh.getBoundingBox(), glm::mat4(1.0f), floorMaterial, false);

    constexpr std::array<glm::vec3, 5> kColors{
        glm::vec3(0.75f, 0.18f, 0.12f),
        glm::vec3(0.15f, 0.38f, 0.78f),
        glm::vec3(0.90f, 0.58f, 0.12f),
        glm::vec3(0.20f, 0.68f, 0.36f),
        glm::vec3(0.62f, 0.25f, 0.72f),
    };

    for (uint32_t index = 0; index < kColors.size(); ++index) {
        const float z = 7.0f - static_cast<float>(index) * 5.0f;
        const float height = 1.5f + static_cast<float>(index) * 0.75f;
        const glm::mat4 transform =
            glm::translate(glm::vec3(-3.0f, height * 0.5f, z)) *
            glm::rotate(glm::radians(10.0f + 13.0f * static_cast<float>(index)), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::vec3(1.5f, height, 1.5f));

        PbrMaterialParams material;
        material.baseColor = kColors[index];
        material.specularRoughness = 0.25f + 0.12f * static_cast<float>(index);
        material.baseMetalness = index == 2 ? 0.65f : 0.0f;
        addObject(fmt::format("pillar-{}", index), cubeGeometry, cubeMesh.getBoundingBox(), transform, material);
    }

    for (uint32_t index = 0; index < kColors.size(); ++index) {
        const float z = 5.0f - static_cast<float>(index) * 5.0f;
        const float radius = 0.8f + 0.15f * static_cast<float>(index);
        const glm::mat4 transform = glm::translate(glm::vec3(3.0f, radius, z)) * glm::scale(glm::vec3(radius));

        PbrMaterialParams material;
        material.baseColor = kColors[kColors.size() - index - 1];
        material.specularRoughness = 0.12f + 0.17f * static_cast<float>(index);
        material.baseMetalness = index % 2 == 0 ? 0.8f : 0.0f;
        addObject(fmt::format("sphere-{}", index), sphereGeometry, sphereMesh.getBoundingBox(), transform, material);
    }
}

void ShadowMappingScene::addObject(
    const std::string_view nodeId,
    Geometry& geometry,
    const BoundingBox3& localBounds,
    const glm::mat4& modelMatrix,
    const PbrMaterialParams& materialParams,
    const bool castsShadow) {
    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.transformPack->M = modelMatrix;

    const PbrMaterial material{.name = std::string{nodeId}, .params = materialParams};
    const auto materialHandle = m_pbrMaterialTable->add(createGpuPbrParams(material, m_resourceContext->imageCache));
    const auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle);

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material = m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;
    forwardPass.setPushConstants(drawParameters);

    if (!castsShadow) {
        return;
    }

    const BoundingBox3 worldBounds = transformBoundingBox(localBounds, modelMatrix);
    for (uint32_t cascadeIndex = 0; cascadeIndex < kDefaultCascadeCount; ++cascadeIndex) {
        auto& shadowPass = node.pass(kCsmPasses[cascadeIndex]);
        shadowPass.setGeometry(&geometry, 0, 1);
        shadowPass.material = m_resourceContext->getMaterial(createShadowMaterialKey(cascadeIndex));
        shadowPass.setPushConstants(drawParameters);

        std::vector<DrawCommand> commands;
        appendDrawCommands(commands, node, kCsmPasses[cascadeIndex]);
        m_shadowDrawCommands[cascadeIndex].push_back({
            .renderNode = &node,
            .worldBounds = worldBounds,
            .command = std::move(commands.back()),
        });
    }
}

void ShadowMappingScene::rebuildDrawCommands() {
    m_forwardDrawCommands.clear();
    m_forwardDrawCommands.reserve(m_renderNodes.size());
    for (const auto& [nodeId, node] : m_renderNodes) {
        appendDrawCommands(m_forwardDrawCommands, *node, kForwardLightingPass);
    }

    std::vector<DrawCommand> skyboxCommands;
    appendDrawCommands(skyboxCommands, m_skybox->getRenderNode(), kForwardLightingPass);
    m_skyboxDrawCommand = std::move(skyboxCommands.front());
}

void ShadowMappingScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](const Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            rebuildDrawCommands();
            break;
        case Key::Space:
            m_animateLight = !m_animateLight;
            break;
        default:
            break;
        }
    }));
}

} // namespace crisp
