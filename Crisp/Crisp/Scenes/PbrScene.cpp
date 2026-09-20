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

constexpr VkQueryPipelineStatisticFlags kPbrGeometryStats =
    VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT;

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

    const auto passId = internRenderPassId(renderPass);
    for (const auto& entry : renderNode.materials) {
        if (entry.passId != passId) {
            continue;
        }
        drawCommands.push_back(entry.createDrawCommand(renderNode));
    }
}

constexpr uint32_t kMeshletTaskWorkGroupSize = 32;

// Mirrors the block in Lighting/pbr-meshlet-draw.part.glsl: the classic path's parameters, then the range this
// dispatch owns.
struct MeshletDrawParameters {
    PbrDrawParameters base{};
    uint32_t firstMeshlet{0};
    uint32_t meshletCount{0};
    uint32_t cullingEnabled{0};
};

static_assert(offsetof(MeshletDrawParameters, firstMeshlet) == sizeof(PbrDrawParameters));
static_assert(sizeof(MeshletDrawParameters) >= 36);

struct MeshletCullParameters {
    uint32_t meshletCount;
    uint32_t cullingEnabled;
};

struct DrawCommandRecordingState {
    const VulkanPipeline* pipeline{nullptr};
    const Material* material{nullptr};
    const Geometry* geometry{nullptr};
    uint8_t firstBuffer{0};
    uint8_t bufferCount{0};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
};

void executeDrawCommand(
    const DrawCommand& command, const VulkanCommandEncoder& commandEncoder, DrawCommandRecordingState& state) {
    if (state.pipeline != command.getPipeline()) {
        commandEncoder.bindPipeline(*command.getPipeline());
        state.pipeline = command.getPipeline();
    }
    commandEncoder.setPushConstants(*command.getPipeline()->getPipelineLayout(), command.pushConstantView.asSpan());

    if (command.material != nullptr && (state.material != command.material || command.dynamicBufferOffsetCount > 0)) {
        commandEncoder.bindDescriptorSets(command.material->getDescriptorSetBinding(command.getDynamicBufferOffsets()));
        state.material = command.material;
    }

    if (state.geometry != command.geometry || state.firstBuffer != command.firstBuffer ||
        state.bufferCount != command.bufferCount) {
        command.geometry->bindVertexBuffers(commandEncoder, command.firstBuffer, command.bufferCount);
        state.geometry = command.geometry;
        state.firstBuffer = command.firstBuffer;
        state.bufferCount = command.bufferCount;
    }

    if (!command.geometryView.isIndexed()) {
        command.drawWithBoundIndexBuffer(commandEncoder);
        return;
    }

    if (state.indexBuffer != command.geometryView.indexBuffer) {
        commandEncoder.bindIndexBuffer(command.geometryView.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        state.indexBuffer = command.geometryView.indexBuffer;
    }
    command.drawWithBoundIndexBuffer(commandEncoder);
}

} // namespace

PbrScene::PbrScene(Renderer* renderer, Window* window, const nlohmann::json& args)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<TargetCameraController>(*m_window);
    if (const auto cameraIt = args.find("camera"); cameraIt != args.end()) {
        const auto target = cameraIt->value("target", std::array{0.0f, 0.0f, 0.0f});
        const auto orientationDegrees = cameraIt->value("orientationDegrees", std::array{30.0f, -15.0f});
        m_cameraController->setTarget(glm::vec3{target[0], target[1], target[2]});
        m_cameraController->setDistance(cameraIt->value("distance", 10.0f));
        m_cameraController->setOrientation(glm::radians(orientationDegrees[0]), glm::radians(orientationDegrees[1]));
    } else {
        m_cameraController->setOrbitDistance(1.0f);
    }
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

            CRISP_TRACE_SCOPE("csm_record");
            const uint32_t statsQueryIndex = getStatsQueryIndex(ctx.virtualFrameIndex, cascadeIndex);
            const bool recordStats = shouldRecordPipelineStats(statsQueryIndex);
            if (recordStats) {
                ctx.commandEncoder.beginQuery(*m_pipelineStatsQueryPool, statsQueryIndex);
            }

            DrawCommandRecordingState recordingState{};
            const glm::mat4 cascadeViewProjection{m_lightSystem->getCascadeViewProjection(cascadeIndex)};
            for (const auto& cached : drawCommands) {
                if (cached.nodeIndex >= nodeCount) {
                    continue;
                }
                if (!cached.renderNode->isVisible) {
                    continue;
                }
                if (!intersectsClipVolume(cascadeViewProjection, cached.worldBounds)) {
                    continue;
                }
                executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
            }
            if (recordStats) {
                ctx.commandEncoder.endQuery(*m_pipelineStatsQueryPool, statsQueryIndex);
                m_pipelineStatsQueryPool->setPending(statsQueryIndex);
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
        CRISP_TRACE_SCOPE("forward_record");
        const uint32_t statsQueryIndex = getStatsQueryIndex(ctx.virtualFrameIndex, kForwardStatsPass);
        const bool recordStats = shouldRecordPipelineStats(statsQueryIndex);
        if (recordStats) {
            ctx.commandEncoder.beginQuery(*m_pipelineStatsQueryPool, statsQueryIndex);
        }

        const bool meshletPathActive = m_useMeshletPath && !m_sceneMeshlets.groups.empty();
        if (meshletPathActive) {
            drawSceneMeshlets(ctx);
        }

        DrawCommandRecordingState recordingState{};
        for (const auto& cached : drawCommands) {
            if (meshletPathActive) {
                break;
            }
            if (cached.nodeIndex >= nodeCount) {
                break;
            }
            if (!cached.renderNode->isVisible) {
                continue;
            }
            CRISP_CHECK_EQ(
                cached.command.getPipeline()->getPipelineLayout(),
                &pbrPipelineLayout,
                "Every draw in the bindless PBR batch must use its pipeline layout; draw special pipelines afterward.");
            executeDrawCommand(cached.command, ctx.commandEncoder, recordingState);
        }

        if (recordStats) {
            ctx.commandEncoder.endQuery(*m_pipelineStatsQueryPool, statsQueryIndex);
            m_pipelineStatsQueryPool->setPending(statsQueryIndex);
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

            const auto meshletCount = static_cast<uint32_t>(m_meshletData.meshlets.size());
            const MeshletCullParameters cullParameters{
                .meshletCount = meshletCount,
                .cullingEnabled = m_cullMeshlets ? 1u : 0u,
            };
            ctx.commandEncoder.setPushConstants(
                *meshPipeline->getPipelineLayout(), VK_SHADER_STAGE_TASK_BIT_EXT, cullParameters);

            const uint32_t taskGroupCount = (meshletCount + kMeshletTaskWorkGroupSize - 1) / kMeshletTaskWorkGroupSize;
            ctx.commandEncoder.drawMeshTasks(taskGroupCount);
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
                key, variant.pipelineConfig, {m_renderGraph->getRasterizationPassDescriptor(kCsmPasses[i])});
            auto* csmMaterial = m_resourceContext->createMaterial(key, csmPipeline);
            csmMaterial->writeDescriptor(1, 0, m_transformBuffer->getStorageDescriptorInfo());
            csmMaterial->writeDescriptor(1, 1, m_lightSystem->getCascadedDirectionalLightBufferInfo(i));
        }
    }

    createPlane();
    m_mergeGeometry = args.value("mergeGeometry", m_mergeGeometry);
    m_optimizeIndices = args.value("optimizeIndices", m_optimizeIndices);
    m_useMeshletPath = args.value("useMeshletPath", m_useMeshletPath);
    createSceneObjects(args.value("modelPath", std::string{}));

    if (args.value("meshletTest", false)) {
        createMeshletTestNode();
        m_drawMeshlets = true;
    }

    m_nodesToDraw = static_cast<int32_t>(m_renderNodes.size());
    rebuildDrawCommandCache();

    m_pipelineStatsQueryPool = std::make_unique<VulkanPipelineStatsQueryPool>(
        m_renderer->getDevice(), kPbrGeometryStats, kRendererVirtualFrameCount * kStatsPassCount, "Pbr Geometry Stats");

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

void PbrScene::beginPipelineStatsFrame(const uint32_t virtualFrameIndex) {
    if (m_pipelineStatsQueryPool == nullptr) {
        return;
    }

    for (uint32_t passIndex = 0; passIndex < kStatsPassCount; ++passIndex) {
        const uint32_t queryIndex = getStatsQueryIndex(virtualFrameIndex, passIndex);
        if (!m_pipelineStatsQueryPool->isPending(queryIndex)) {
            continue;
        }
        if (!m_pipelineStatsQueryPool->tryGetResults(m_pipelineStats[passIndex], queryIndex)) {
            continue;
        }
        m_pipelineStatsQueryPool->reset(queryIndex);
    }
}

bool PbrScene::shouldRecordPipelineStats(const uint32_t queryIndex) const {
    return m_collectPipelineStats && m_pipelineStatsQueryPool != nullptr &&
           !m_pipelineStatsQueryPool->isPending(queryIndex);
}

void PbrScene::render(const FrameContext& frameContext) {
    CRISP_TRACE_VK_SCOPE("PbrScene::render", frameContext.commandEncoder);

    beginPipelineStatsFrame(frameContext.virtualFrameIndex);

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
        if (!m_sceneMeshlets.groups.empty()) {
            ImGui::Checkbox("Meshlet Path", &m_useMeshletPath);
            ImGui::Checkbox("Cull Scene Meshlets", &m_cullSceneMeshlets);

            const glm::vec3 cameraPosition{m_cameraController->getCamera().getPosition()};
            uint32_t visible{0};
            for (const auto& bounds : m_sceneMeshlets.bounds) {
                visible += isMeshletConeVisible(bounds, cameraPosition) ? 1u : 0u;
            }
            const auto total = static_cast<uint32_t>(m_sceneMeshlets.bounds.size());
            ImGui::Text(
                "Scene clusters cone culled: %u of %u (%.1f%%)",
                total - visible,
                total,
                total == 0 ? 0.0f : 100.0f * static_cast<float>(total - visible) / static_cast<float>(total));
        }
        if (!m_meshletData.meshlets.empty()) {
            ImGui::Checkbox("Draw Meshlets", &m_drawMeshlets);
            ImGui::Checkbox("Cone Cull Meshlets", &m_cullMeshlets);

            const glm::vec3 cameraPosition{m_cameraController->getCamera().getPosition()};
            uint32_t visibleMeshlets{0};
            for (const auto& bounds : m_meshletData.bounds) {
                visibleMeshlets += isMeshletConeVisible(bounds, cameraPosition) ? 1u : 0u;
            }
            const auto meshletCount = static_cast<uint32_t>(m_meshletData.meshlets.size());
            ImGui::Text(
                "Cone culled: %u of %u (%.1f%%)",
                meshletCount - visibleMeshlets,
                meshletCount,
                meshletCount == 0
                    ? 0.0f
                    : 100.0f * static_cast<float>(meshletCount - visibleMeshlets) / static_cast<float>(meshletCount));
        }
    }
    if (ImGui::CollapsingHeader("Pipeline Stats")) {
        ImGui::Checkbox("Collect", &m_collectPipelineStats);

        const auto extent = m_renderer->getSwapChainExtent();
        const auto pixels = static_cast<double>(extent.width) * extent.height;
        const auto toMillions = [](const std::optional<uint64_t>& value) {
            return static_cast<double>(value.value_or(0)) / 1.0e6;
        };

        if (ImGui::BeginTable("pipelineStats", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("IA prims (M)");
            ImGui::TableSetupColumn("Rasterized (M)");
            ImGui::TableSetupColumn("Culled");
            ImGui::TableSetupColumn("VS inv (M)");
            ImGui::TableSetupColumn("FS inv (M)");
            ImGui::TableHeadersRow();

            for (uint32_t passIndex = 0; passIndex < kStatsPassCount; ++passIndex) {
                const auto& stats = m_pipelineStats[passIndex];
                const double submitted = toMillions(stats.inputAssemblyPrimitives);
                const double rasterized = toMillions(stats.clippingPrimitives);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (passIndex == kForwardStatsPass) {
                    ImGui::TextUnformatted("forward");
                } else {
                    ImGui::Text("csm%u", passIndex);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%8.3f", submitted);
                ImGui::TableNextColumn();
                ImGui::Text("%8.3f", rasterized);
                ImGui::TableNextColumn();
                ImGui::Text("%5.1f%%", submitted == 0.0 ? 0.0 : 100.0 * (1.0 - rasterized / submitted));
                ImGui::TableNextColumn();
                ImGui::Text("%8.3f", toMillions(stats.vertexShaderInvocations));
                ImGui::TableNextColumn();
                ImGui::Text("%8.3f", toMillions(stats.fragmentShaderInvocations));
            }
            ImGui::EndTable();
        }

        const auto forwardFragments =
            static_cast<double>(m_pipelineStats[kForwardStatsPass].fragmentShaderInvocations.value_or(0));
        ImGui::Text("Forward overdraw: %.2fx over %.0f pixels", pixels > 0.0 ? forwardFragments / pixels : 0.0, pixels);
        ImGui::TextDisabled("Forward row covers the PBR batch only; skybox and meshlets draw after the query.");
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
            return cached.command.getPipeline();
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
        "pbr", "PbrTex.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});

    setEnvironmentMap("GreenwichPark");
    imageCache.addImage("brdfLut", loadBrdfLut(m_renderer));

    m_forwardPassMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 1, 1);
    configureForwardLightingPassMaterial(*m_forwardPassMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);

    m_pbrMaterialTable = std::make_unique<PbrMaterialTable>(m_renderer->getDevice(), kMaximumObjectCount);
    m_pbrDrawMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 2, 1);
    m_pbrDrawMaterial->writeDescriptor(2, 0, m_transformBuffer->getStorageDescriptorInfo());
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
    auto [images, models] = loadGltfAsset(path, {.optimizeIndices = m_optimizeIndices}).unwrap();
    CRISP_LOGI("Loaded {} models from {}.", models.size(), path.generic_string());

    addPbrImageGroupToImageCache(images, m_resourceContext->imageCache);

    const auto modelName = path.stem().string();

    Geometry* mergedGeometry{nullptr};
    if (m_mergeGeometry) {
        std::vector<const TriangleMesh*> meshes;
        meshes.reserve(models.size());
        for (const auto& model : models) {
            meshes.push_back(&model.mesh);
        }
        mergedGeometry = &m_resourceContext->addGeometry(
            fmt::format("{}_merged", modelName),
            createMergedGeometry(*m_renderer, meshes, kPbrVertexFormat, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    }

    std::vector<SceneMeshletSource> meshletSources;
    meshletSources.reserve(models.size());
    uint32_t vertexBase{0};
    for (auto&& [idx, model] : std::views::enumerate(models)) {
        const auto& node = addSceneObject(
            fmt::format("{}_{}", modelName, idx),
            model.mesh,
            model.material,
            model.transform,
            mergedGeometry,
            static_cast<int32_t>(idx));

        meshletSources.push_back(
            SceneMeshletSource{
                .mesh = &model.mesh,
                .modelMatrix = model.transform,
                .vertexBase = vertexBase,
                .transformIndex = node.transformHandle.index,
                .materialIndex =
                    addOrReuseMaterial(createGpuPbrParams(model.material, m_resourceContext->imageCache)).index,
            });
        vertexBase += model.mesh.getVertexCount();
    }

    if (mergedGeometry != nullptr) {
        buildSceneMeshlets(meshletSources);
        createMeshletResources(*mergedGeometry);
    }
}

void PbrScene::createObjSceneObject(const std::filesystem::path& path) {
    const auto mesh = loadTriangleMesh(path).unwrap();

    PbrMaterial material{};
    material.name = path.stem().string();
    material.params.surface.baseColor = glm::vec3(0.5f);

    addSceneObject(
        material.name, mesh, material, glm::translate(glm::vec3(0.0f, kFloorHeight - mesh.getBoundingBox().min.y, 0.0f)));
}

PbrMaterialHandle PbrScene::addOrReuseMaterial(const PbrMaterialParams& params) {
    const std::string key{reinterpret_cast<const char*>(&params), sizeof(PbrMaterialParams)}; // NOLINT
    if (const auto it = m_materialHandles.find(key); it != m_materialHandles.end()) {
        return it->second;
    }

    const auto handle = m_pbrMaterialTable->add(params);
    m_materialHandles.emplace(key, handle);
    return handle;
}

RenderNode& PbrScene::addSceneObject(
    const std::string_view nodeId,
    const TriangleMesh& mesh,
    const PbrMaterial& material,
    const glm::mat4& modelMatrix,
    Geometry* sharedGeometry,
    const int32_t geometryPartIndex) {
    auto& geometry =
        sharedGeometry != nullptr
            ? *sharedGeometry
            : m_resourceContext->addGeometry(nodeId, createGeometry(*m_renderer, mesh, kPbrVertexFormat));

    auto& node = createRenderNode(nodeId);
    node.geometry = &geometry;
    node.geometryPartIndex = sharedGeometry != nullptr ? geometryPartIndex : -1;
    node.transformPack->M = modelMatrix;
    m_renderNodeWorldBounds.emplace(&node, transformBoundingBox(mesh.getBoundingBox(), modelMatrix));

    auto& forwardPass = node.pass(kForwardLightingPass);
    forwardPass.material = m_pbrDrawMaterial.get();
    forwardPass.transformBufferDynamicIndex = 0;
    const auto gpuMaterial = createGpuPbrParams(material, m_resourceContext->imageCache);
    const auto materialHandle = addOrReuseMaterial(gpuMaterial);
    auto drawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle);
    drawParameters.transformIndex = node.transformHandle.index;
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

    return node;
}

void PbrScene::buildSceneMeshlets(const std::span<const SceneMeshletSource> sources) {
    struct PendingMeshlet {
        Meshlet meshlet;
        MeshletBounds bounds;
        uint32_t transformIndex;
        uint32_t materialIndex;
    };

    std::vector<PendingMeshlet> pending;
    m_sceneMeshlets = {};

    for (const auto& source : sources) {
        CRISP_CHECK(source.mesh != nullptr);
        const auto meshMeshlets = buildMeshlets(*source.mesh);
        if (meshMeshlets.meshlets.empty()) {
            continue;
        }

        const auto vertexArrayBase = static_cast<uint32_t>(m_sceneMeshlets.vertices.size());

        m_sceneMeshlets.triangles.resize((m_sceneMeshlets.triangles.size() + 3) & ~size_t{3});
        const auto triangleArrayBase = static_cast<uint32_t>(m_sceneMeshlets.triangles.size());

        for (const auto vertexIndex : meshMeshlets.vertices) {
            m_sceneMeshlets.vertices.push_back(vertexIndex + source.vertexBase);
        }
        m_sceneMeshlets.triangles.insert(
            m_sceneMeshlets.triangles.end(), meshMeshlets.triangles.begin(), meshMeshlets.triangles.end());

        for (size_t i = 0; i < meshMeshlets.meshlets.size(); ++i) {
            Meshlet meshlet{meshMeshlets.meshlets[i]};
            meshlet.vertexOffset += vertexArrayBase;
            meshlet.triangleOffset += triangleArrayBase;
            pending.push_back(
                PendingMeshlet{
                    .meshlet = meshlet,
                    .bounds = meshMeshlets.bounds[i].transformedBy(source.modelMatrix),
                    .transformIndex = source.transformIndex,
                    .materialIndex = source.materialIndex,
                });
        }
    }

    std::ranges::stable_sort(pending, {}, &PendingMeshlet::materialIndex);

    m_sceneMeshlets.meshlets.reserve(pending.size());
    m_sceneMeshlets.bounds.reserve(pending.size());
    m_sceneMeshlets.transformIndices.reserve(pending.size());
    for (const auto& entry : pending) {
        if (m_sceneMeshlets.groups.empty() ||
            m_sceneMeshlets.groups.back().drawParameters.materialIndex != entry.materialIndex) {
            auto drawParameters = m_pbrMaterialTable->createDrawParameters(PbrMaterialHandle{entry.materialIndex});
            m_sceneMeshlets.groups.push_back(
                SceneMeshlets::MaterialGroup{
                    .firstMeshlet = static_cast<uint32_t>(m_sceneMeshlets.meshlets.size()),
                    .meshletCount = 0,
                    .drawParameters = drawParameters,
                });
        }

        ++m_sceneMeshlets.groups.back().meshletCount;
        m_sceneMeshlets.meshlets.push_back(entry.meshlet);
        m_sceneMeshlets.bounds.push_back(entry.bounds);
        m_sceneMeshlets.transformIndices.push_back(entry.transformIndex);
    }

    CRISP_LOGI(
        "Scene meshlets: {} clusters over {} objects in {} material groups ({:.1f} KB of cluster data).",
        m_sceneMeshlets.meshlets.size(),
        sources.size(),
        m_sceneMeshlets.groups.size(),
        static_cast<double>(
            m_sceneMeshlets.meshlets.size() * sizeof(Meshlet) + m_sceneMeshlets.bounds.size() * sizeof(MeshletBounds) +
            m_sceneMeshlets.vertices.size() * sizeof(uint32_t) + m_sceneMeshlets.triangles.size()) /
            1024.0);
}

void PbrScene::drawSceneMeshlets(const FrameContext& ctx) {
    auto* pipeline = m_resourceContext->pipelineCache.getPipeline("pbrMeshlet");
    ctx.commandEncoder.bindPipeline(*pipeline);

    auto& bindlessRegistry = m_renderer->getBindlessImageRegistry();
    bindlessRegistry.bind(
        ctx.commandEncoder,
        pipeline->getPipelineLayout()->getHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        BindlessImageRegistry::kGlobalSetIndex);
    ctx.commandEncoder.bindDescriptorSets(m_meshletMaterial->getDescriptorSetBinding());

    for (const auto& group : m_sceneMeshlets.groups) {
        MeshletDrawParameters drawParameters{};
        drawParameters.base = group.drawParameters;
        drawParameters.firstMeshlet = group.firstMeshlet;
        drawParameters.meshletCount = group.meshletCount;
        drawParameters.cullingEnabled = m_cullSceneMeshlets ? 1u : 0u;
        ctx.commandEncoder.setPushConstants(
            *pipeline->getPipelineLayout(),
            std::span{reinterpret_cast<const std::byte*>(&drawParameters), sizeof(drawParameters)}); // NOLINT

        const uint32_t taskGroupCount = (group.meshletCount + kMeshletTaskWorkGroupSize - 1) / kMeshletTaskWorkGroupSize;
        ctx.commandEncoder.drawMeshTasks(taskGroupCount);
    }
}

void PbrScene::createMeshletResources(const Geometry& mergedGeometry) {
    if (m_sceneMeshlets.meshlets.empty()) {
        return;
    }

    auto* meshletBuffer = m_resourceContext->createStorageBuffer("sceneMeshlets", m_sceneMeshlets.meshlets);
    auto* vertexBuffer = m_resourceContext->createStorageBuffer("sceneMeshletVertices", m_sceneMeshlets.vertices);
    auto* triangleBuffer = m_resourceContext->createStorageBuffer("sceneMeshletTriangles", m_sceneMeshlets.triangles);
    auto* boundsBuffer = m_resourceContext->createStorageBuffer("sceneMeshletBounds", m_sceneMeshlets.bounds);
    auto* transformIndexBuffer =
        m_resourceContext->createStorageBuffer("sceneMeshletTransforms", m_sceneMeshlets.transformIndices);

    auto* pipeline = m_resourceContext->createPipeline(
        "pbrMeshlet", "PbrMeshlet.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    m_meshletMaterial =
        std::make_unique<Material>(pipeline, pipeline->getPipelineLayout()->getVulkanDescriptorSetAllocator(), 1, 3);
    configureForwardLightingPassMaterial(*m_meshletMaterial, *m_resourceContext, *m_lightSystem, *m_renderGraph);
    m_meshletMaterial->writeDescriptor(2, 0, m_transformBuffer->getStorageDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 0, meshletBuffer->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 1, vertexBuffer->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 2, triangleBuffer->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 3, boundsBuffer->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 4, transformIndexBuffer->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 5, mergedGeometry.getVertexBuffer(0)->createDescriptorInfo());
    m_meshletMaterial->writeDescriptor(3, 6, mergedGeometry.getVertexBuffer(1)->createDescriptorInfo());
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
    auto floorDrawParameters = m_pbrMaterialTable->createDrawParameters(materialHandle);
    floorDrawParameters.transformIndex = floor.transformHandle.index;
    forwardPass.setPushConstants(floorDrawParameters);

    CRISP_CHECK(
        floor.pass(kForwardLightingPass)
            .material->getPipeline()
            ->getVertexLayout()
            .isSubsetOf(floor.geometry->getVertexLayout()));
}

void PbrScene::createMeshletTestNode() {
    constexpr std::string_view kNodeName{"meshletTest"};

    auto [mesh, materials, meshletData] =
        loadTriangleMeshlets(m_renderer->getResourcesPath() / "Models/bunny.obj").unwrap();
    m_meshletData = std::move(meshletData);

    // The mesh shader reads positions and attributes straight out of the vertex buffers, so they need storage usage.
    auto& geometry = m_resourceContext->addGeometry(
        kNodeName, createGeometry(*m_renderer, mesh, kPbrVertexFormat, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));

    auto* meshletBuffer = m_resourceContext->createStorageBuffer("meshletBuffer", m_meshletData.meshlets);
    auto* meshletVertices = m_resourceContext->createStorageBuffer("meshletVertices", m_meshletData.vertices);
    auto* meshletTriangles = m_resourceContext->createStorageBuffer("meshletTriangles", m_meshletData.triangles);
    auto* meshletBounds = m_resourceContext->createStorageBuffer("meshletBounds", m_meshletData.bounds);

    auto* meshPipeline = m_resourceContext->createPipeline(
        "mesh", "MeshShading.json", {m_renderGraph->getRasterizationPassDescriptor(kForwardLightingPass)});
    auto* meshMaterial = m_resourceContext->createMaterial("mesh", meshPipeline);
    meshMaterial->writeDescriptor(0, 0, meshletBuffer->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 1, meshletTriangles->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 2, meshletVertices->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 3, geometry.getVertexBuffer(0)->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 4, m_resourceContext->getRingBuffer("camera")->getDescriptorInfo());
    meshMaterial->writeDescriptor(0, 5, geometry.getVertexBuffer(1)->createDescriptorInfo());
    meshMaterial->writeDescriptor(0, 6, meshletBounds->createDescriptorInfo());
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
