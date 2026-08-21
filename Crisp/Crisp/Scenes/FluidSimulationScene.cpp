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
            const auto& sphData = builder.getBlackboard().get<SphPassData>();
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
        ImGui::Checkbox("Advance by real frame time", &params.useRealFrameTime);
        if (!params.useRealFrameTime) {
            float rateHz = 1.0f / std::max(params.simulatedTimePerFrame, 1e-4f);
            if (ImGui::SliderFloat("Simulated rate", &rateHz, 15.0f, 240.0f, "%.0f Hz")) {
                params.simulatedTimePerFrame = 1.0f / rateHz;
            }
        }
        // The chain is baked into the graph once per substep, so changing the count rebuilds it.
        int substepCount = static_cast<int>(m_fluidSimulation->getSubstepCount());
        if (ImGui::SliderInt("Substeps per frame", &substepCount, 1, 32)) {
            m_renderer->finish();
            m_fluidSimulation->setSubstepCount(static_cast<uint32_t>(substepCount));
            buildRenderGraph();
        }
        ImGui::SliderFloat("Max substep", &params.maxSubstepTime, 1e-4f, 5e-3f, "%.4f s", ImGuiSliderFlags_Logarithmic);
        ImGui::TextDisabled("Substep: %.4f s", m_fluidSimulation->getSubstepTime()); // NOLINT

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
