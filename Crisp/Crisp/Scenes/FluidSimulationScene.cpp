#include <Crisp/Scenes/FluidSimulationScene.hpp>

#include <algorithm>

#include <imgui.h>

#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>

namespace crisp {
namespace {
constexpr const char* kParticlePass = "fluid-particles";
constexpr const char* kTransformBufferId = "fluidTransforms";
constexpr const char* kParticleBufferId = "fluidParticleParams";

struct FluidParticlesPassData {
    RenderGraphResourceHandle image;
};
} // namespace

FluidSimulationScene::FluidSimulationScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformRingBuffer<TransformPack>(kTransformBufferId);
    m_resourceContext->createUniformRingBuffer<ParticleParams>(kParticleBufferId);

    m_fluidSimulation = std::make_unique<SPH>(*m_renderer, SphConfig{});

    m_particleGeometry = &m_resourceContext->addGeometry("fluidParticles", Geometry{});
    m_particleGeometry->addNonOwningVertexBuffer(&m_fluidSimulation->getPositionBuffer());
    m_particleGeometry->addNonOwningVertexBuffer(&m_fluidSimulation->getColorBuffer());
    m_particleGeometry->setVertexCount(m_fluidSimulation->getParticleCount());
    m_particleGeometry->setInstanceCount(1);

    resetCamera();
    buildRenderGraph();

    m_renderer->getDevice().flushDescriptorUpdates();
}

FluidSimulationScene::~FluidSimulationScene() = default;

void FluidSimulationScene::buildRenderGraph() {
    m_renderGraph = std::make_unique<rg::RenderGraph>();
    m_fluidSimulation->addComputePasses(*m_renderGraph);

    m_renderGraph->addPass(
        kParticlePass,
        PassType::Rasterizer,
        [](rg::RenderGraph::Builder& builder) {
            const auto& sphData = builder.getBlackboard().get<WcsphPassData>();
            builder.readBuffer(sphData.positions, kVertexInputRead);
            builder.readBuffer(sphData.colors, kVertexInputRead);

            auto& data = builder.getBlackboard().insert<FluidParticlesPassData>();
            data.image = builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                },
                fmt::format("{}-color", kParticlePass),
                VkClearValue{.color{{0.05f, 0.05f, 0.08f, 1.0f}}});
            builder.exportTexture(data.image);

            builder.createAttachment(
                {
                    .sizePolicy = SizePolicy::SwapChainRelative,
                    .format = VK_FORMAT_D32_SFLOAT,
                },
                fmt::format("{}-depth", kParticlePass),
                VkClearValue{.depthStencil{0.0f, 0}});
        },
        [this](const FrameContext& ctx) {
            ctx.commandEncoder.bindPipeline(*m_pointSpritePipeline);
            ctx.commandEncoder.bindDescriptorSets(m_pointSpriteMaterial->getDescriptorSetBinding());
            m_particleGeometry->bindAndDraw(ctx.commandEncoder);
        });

    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());

    m_pointSpritePipeline = m_resourceContext->createPipeline(
        "pointSprite", "PointSprite.json", m_renderGraph->getRasterizationPassDescriptor(kParticlePass));
    m_pointSpriteMaterial = m_resourceContext->createMaterial("pointSprite", m_pointSpritePipeline);
    m_pointSpriteMaterial->writeDescriptor(0, 0, *m_resourceContext->getRingBuffer(kTransformBufferId));
    m_pointSpriteMaterial->writeDescriptor(1, 0, *m_resourceContext->getRingBuffer(kParticleBufferId));

    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&FluidParticlesPassData::image>());
}

void FluidSimulationScene::resetCamera() {
    const glm::vec3 boxSize = m_fluidSimulation->getFluidSpaceSize() * m_vizScale;
    const float boxExtent = std::max({boxSize.x, boxSize.y, boxSize.z});
    m_cameraController->setPosition({0.5f * boxSize.x, 0.5f * boxSize.y, 2.0f * boxExtent});
    m_cameraController->setSpeed(boxExtent);
}

void FluidSimulationScene::resize(const int width, const int height) {
    m_cameraController->onViewportResized(width, height);
    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&FluidParticlesPassData::image>());
}

void FluidSimulationScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    m_fluidSimulation->update(updateParams.dt);
    m_vizTimeDelta = m_vizTimeDelta == 0.0f ? updateParams.dt : glm::mix(m_vizTimeDelta, updateParams.dt, 0.05f);

    const auto& camera = m_cameraController->getCamera();
    m_transforms.M = glm::scale(glm::vec3(m_vizScale));
    m_transforms.MV = camera.getViewMatrix() * m_transforms.M;
    m_transforms.MVP = camera.getProjectionMatrix() * m_transforms.MV;
    m_transforms.N = glm::inverse(glm::transpose(m_transforms.MV));

    // The sprite has to cover a sphere of this world-space radius, and gl_PointSize is in pixels:
    // radius * screenSpaceScale / viewDepth is the projected diameter.
    m_particleParams.radius = m_fluidSimulation->getParticleRadius() * m_vizScale;
    m_particleParams.screenSpaceScale =
        static_cast<float>(m_renderer->getSwapChainExtent().width) * camera.getProjectionMatrix()[0][0];

    m_resourceContext->getRingBuffer(kTransformBufferId)
        ->updateStagingBufferFromStruct(m_transforms, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer(kParticleBufferId)
        ->updateStagingBufferFromStruct(m_particleParams, updateParams.frameInFlightIdx);
}

void FluidSimulationScene::render(const FrameContext& frameContext) {
    constexpr auto kUniformReads = kVertexUniformRead | kFragmentUniformRead;
    frameContext.commandEncoder.insertBarrier(kUniformReads >> kTransferWrite);
    m_resourceContext->getRingBuffer(kTransformBufferId)->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer(kParticleBufferId)->updateDeviceBuffer(frameContext.commandEncoder);
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kUniformReads);

    m_renderGraph->execute(frameContext);
}

void FluidSimulationScene::drawStageTimings() const {
    const auto names = SPH::getStageNames();
    const auto timings = m_fluidSimulation->getStageTimingsMs();
    const auto substeps = m_fluidSimulation->getActiveSubstepCount();

    double frameTotal = 0.0;
    for (size_t i = 0; i < names.size() && i < timings.size(); ++i) {
        frameTotal += timings[i].value_or(0.0) * substeps;
    }

    constexpr auto kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##SolverStages", 4, kFlags)) {
        return;
    }

    ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("ms/substep", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("ms/frame", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableHeadersRow();

    for (size_t i = 0; i < names.size() && i < timings.size(); ++i) {
        const double perSubstep = timings[i].value_or(0.0);
        const double perFrame = perSubstep * substeps;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(names[i]);
        ImGui::TableNextColumn();
        if (timings[i]) {
            ImGui::Text("%.4f", perSubstep); // NOLINT
        } else {
            ImGui::TextDisabled("-"); // NOLINT
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", perFrame); // NOLINT
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", frameTotal > 0.0 ? 100.0 * perFrame / frameTotal : 0.0); // NOLINT
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("total");
    ImGui::TableNextColumn();
    ImGui::TextDisabled("x%u", substeps); // NOLINT
    ImGui::TableNextColumn();
    ImGui::Text("%.3f", frameTotal); // NOLINT
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("100.0");

    ImGui::EndTable();
}

void FluidSimulationScene::drawGui() {
    ImGui::SetNextWindowSize(ImVec2(440.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Fluid Simulation")) {
        auto& params = m_fluidSimulation->getParameters();

        bool paused = m_fluidSimulation->isPaused();
        if (ImGui::Checkbox("Paused (Space)", &paused)) {
            m_fluidSimulation->setPaused(paused);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset (R)")) {
            m_fluidSimulation->reset();
        }

        ImGui::SliderFloat3("Gravity", &params.gravity.x, -20.0f, 20.0f, "%.2f m/s^2");
        ImGui::SliderFloat("Viscosity", &params.viscosity, 0.0f, 30.0f, "%.2f");
        ImGui::SliderFloat("Surface tension", &params.kappa, 0.0f, 50.0f, "%.2f");

        ImGui::Separator();
        ImGui::Checkbox("Fixed frame time", &params.useFixedFrameTime);
        if (params.useFixedFrameTime) {
            float rateHz = 1.0f / std::max(params.simulatedTimePerFrame, 1e-4f);
            if (ImGui::SliderFloat("Simulated rate", &rateHz, 15.0f, 240.0f, "%.0f Hz")) {
                params.simulatedTimePerFrame = 1.0f / rateHz;
            }
        } else {
            ImGui::SliderFloat("Time scale", &params.timeScale, 0.05f, 2.0f, "%.2fx");
        }
        ImGui::SliderFloat(
            "Target substep", &params.targetSubstepTime, 1e-4f, 5e-3f, "%.4f s", ImGuiSliderFlags_Logarithmic);

        int maxSubstepCount = static_cast<int>(m_fluidSimulation->getMaxSubstepCount());
        if (ImGui::SliderInt("Max substeps", &maxSubstepCount, 1, 64)) {
            m_renderer->finish();
            m_fluidSimulation->setMaxSubstepCount(static_cast<uint32_t>(maxSubstepCount));
            buildRenderGraph();
        }
        ImGui::TextDisabled( // NOLINT
            "Substeps: %u of %u at %.4f s",
            m_fluidSimulation->getActiveSubstepCount(),
            m_fluidSimulation->getMaxSubstepCount(),
            m_fluidSimulation->getSubstepTime());

        const float simulated = m_fluidSimulation->getSimulatedTimePerFrame();
        const float realTimeFraction = m_vizTimeDelta > 0.0f ? simulated / m_vizTimeDelta : 0.0f;
        ImGui::TextDisabled( // NOLINT
            "Simulated: %.2f ms/frame  (%.0f%% of real time)", simulated * 1000.0f, 100.0f * realTimeFraction);
        if (realTimeFraction < 0.95f && !params.useFixedFrameTime) {
            ImGui::TextDisabled("  substep ceiling is capping it: raise Max substeps"); // NOLINT
        }
        if (realTimeFraction > 1.05f) {
            ImGui::TextDisabled("  faster than real time by choice"); // NOLINT
        }
        if (ImGui::CollapsingHeader("Solver stages")) {
            drawStageTimings();
        }

        ImGui::Separator();
        ImGui::SliderFloat("Visualization scale", &m_vizScale, 1.0f, 50.0f, "%.1f");
        if (ImGui::Button("Reset camera")) {
            resetCamera();
        }

        const glm::vec3 boxSize = m_fluidSimulation->getFluidSpaceSize();
        ImGui::TextDisabled("%u particles", m_fluidSimulation->getParticleCount());        // NOLINT
        ImGui::TextDisabled("Box: %.2f x %.2f x %.2f m", boxSize.x, boxSize.y, boxSize.z); // NOLINT
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    ImGui::End();
}

void FluidSimulationScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](const Key key, int) {
        switch (key) { // NOLINT
        case Key::Space:
            m_fluidSimulation->setPaused(!m_fluidSimulation->isPaused());
            break;
        case Key::R:
            m_fluidSimulation->reset();
            break;
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        default:
            break;
        }
    }));
}
} // namespace crisp
