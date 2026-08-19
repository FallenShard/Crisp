#include <Crisp/Scenes/AtmosphereScene.hpp>

#include <imgui.h>

#include <Crisp/Models/AtmosphereGui.hpp>
#include <Crisp/Renderer/RenderGraph/RenderGraphGui.hpp>

namespace crisp {
namespace {
const auto logger = createLoggerMt("AtmosphereScene");

void drawTooltip(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)"); // NOLINT
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool beginSection(const char* label) {
    return ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
}
} // namespace

AtmosphereScene::AtmosphereScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_cameraController->setSpeed(m_cameraSpeed);
    m_cameraController->setPosition({0.0f, 1000.0f, 0.0f}); // In meters.
    m_resourceContext->createUniformRingBuffer<CameraParameters>("camera");
    m_resourceContext->createUniformRingBuffer<AtmosphereParameters>("atmosphereBuffer");
    m_resourceContext->createUniformRingBuffer<TonemapParameters>(kTonemapBufferId);

    applyAtmosphereSettings(m_settings, m_atmosphereParams);

    m_renderGraph = std::make_unique<rg::RenderGraph>();
    addAtmosphereRenderPasses(*m_renderGraph, *m_renderer, *m_resourceContext, m_atmosphereMaterials);
    addTonemapPass(
        *m_renderGraph, *m_renderer, *m_resourceContext, m_renderGraph->getBlackboard().get<AtmospherePassData>().image);
    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    createAtmosphereRenderMaterials(m_atmosphereMaterials, *m_renderGraph, *m_renderer, *m_resourceContext);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&TonemapPassData::image>());

    m_renderer->getDevice().flushDescriptorUpdates();
}

void AtmosphereScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    createAtmosphereRenderMaterials(m_atmosphereMaterials, *m_renderGraph, *m_renderer, *m_resourceContext);
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&TonemapPassData::image>());
}

void AtmosphereScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    const auto screenExtent = m_renderer->getSwapChainExtent();
    m_atmosphereParams.screenResolution = glm::vec2(screenExtent.width, screenExtent.height);
    m_atmosphereParams.VP = camParams.P * camParams.V;
    m_atmosphereParams.invVP = glm::inverse(m_atmosphereParams.VP);
    m_atmosphereParams.cameraPosition = m_cameraController->getCamera().getPosition() / kMetersPerKilometer;
    applyAtmosphereSettings(m_settings, m_atmosphereParams);

    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(camParams, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer("atmosphereBuffer")
        ->updateStagingBufferFromStruct(m_atmosphereParams, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer(kTonemapBufferId)
        ->updateStagingBufferFromStruct(m_tonemapParams, updateParams.frameInFlightIdx);
}

void AtmosphereScene::render(const FrameContext& frameContext) {
    constexpr auto kUniformReads = kComputeUniformRead | kFragmentUniformRead;
    frameContext.commandEncoder.insertBarrier(kUniformReads >> kTransferWrite);
    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer("atmosphereBuffer")->updateDeviceBuffer(frameContext.commandEncoder);
    m_resourceContext->getRingBuffer(kTonemapBufferId)->updateDeviceBuffer(frameContext.commandEncoder);
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kUniformReads);

    m_renderGraph->execute(frameContext);
}

void AtmosphereScene::drawGui() {
    drawAtmosphereGui();
    drawTonemapGui();

    ImGui::SetNextWindowSize(ImVec2(440.0f, 500.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Render Graph")) {
        drawRenderGraphGui(*m_renderGraph);
    }
    ImGui::End();
}

void AtmosphereScene::drawTonemapGui() {
    ImGui::SetNextWindowSize(ImVec2(440.0f, 320.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Tonemapping")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reset")) {
        m_tonemapParams = TonemapParameters{};
    }

    ImGui::Combo(
        "Operator",
        &m_tonemapParams.operatorIndex,
        kTonemapOperatorNames.data(),
        static_cast<int32_t>(kTonemapOperatorNames.size()));
    drawTooltip(
        "None clamps to [0, 1] after exposure, which is what this scene did before the pass existed. The "
        "others roll the highlights off instead of clipping them.");

    ImGui::SliderFloat("Exposure", &m_tonemapParams.exposure, 0.01f, 1000.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    drawTooltip(
        "Linear scale on scene radiance before the curve. The ray march now writes raw radiance, so this is "
        "the only place brightness is set.");

    if (m_tonemapParams.operatorIndex == 1) {
        ImGui::SliderFloat(
            "White point", &m_tonemapParams.whitePoint, 0.1f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        drawTooltip("Exposed radiance that maps to 1. Anything brighter is allowed to clip.");
    }

    if (m_tonemapParams.operatorIndex == 3) {
        ImGui::SliderFloat("Contrast", &m_tonemapParams.contrast, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Linear start", &m_tonemapParams.linearStart, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Linear length", &m_tonemapParams.linearLength, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Black tightness", &m_tonemapParams.blackTightness, 1.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Pedestal", &m_tonemapParams.pedestal, 0.0f, 1.0f, "%.3f");
        drawTooltip("Lifts the blacks. 0 keeps them crushed to zero.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Output is linear; sRGB encode stays in the present pass."); // NOLINT

    ImGui::End();
}

void AtmosphereScene::drawAtmosphereGui() { // NOLINT(readability-function-size)
    ImGui::SetNextWindowSize(ImVec2(440.0f, 700.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Atmosphere")) {
        ImGui::End();
        return;
    }

    drawAtmosphereGuiContents(m_settings, m_atmosphereParams);

    if (beginSection("Camera")) {
        if (ImGui::SliderFloat("Speed", &m_cameraSpeed, 1.0f, 1e5f, "%.0f m/s", ImGuiSliderFlags_Logarithmic)) {
            m_cameraController->setSpeed(m_cameraSpeed);
        }
        float fovY = m_cameraController->getCamera().getVerticalFov();
        if (ImGui::SliderFloat("Vertical FOV", &fovY, 5.0f, 90.0f, "%.1f deg")) {
            m_cameraController->setFovY(fovY);
        }
        const glm::vec3 pos = m_cameraController->getCamera().getPosition();
        ImGui::TextDisabled("Position: (%.1f, %.1f, %.1f) m", pos.x, pos.y, pos.z); // NOLINT
        // In km, to read against skyViewLutMaxAltitude and the planet radii.
        ImGui::TextDisabled("Altitude: %.3f km", m_atmosphereParams.cameraPosition.y); // NOLINT
    }

    ImGui::End();
}

void AtmosphereScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](Key key, int) {
        switch (key) { // NOLINT
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        }
    }));
}
} // namespace crisp
