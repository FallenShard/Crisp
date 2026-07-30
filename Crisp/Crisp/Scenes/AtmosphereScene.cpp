#include <Crisp/Scenes/AtmosphereScene.hpp>

#include <imgui.h>

namespace crisp {
namespace {
const auto logger = createLoggerMt("AtmosphereScene");

float azimuth = 0.0f;
float altitude = 0.0f;

} // namespace

AtmosphereScene::AtmosphereScene(Renderer* renderer, Window* window)
    : Scene(renderer, window) {
    setupInput();

    m_cameraController = std::make_unique<FreeCameraController>(*m_window);
    m_resourceContext->createUniformRingBuffer<CameraParameters>("camera");
    m_resourceContext->createUniformRingBuffer<AtmosphereParameters>("atmosphereBuffer");

    m_renderGraph = std::make_unique<rg::RenderGraph>();
    addAtmosphereRenderPasses(*m_renderGraph, *m_renderer, *m_resourceContext);
    m_renderGraph->compile(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&AtmospherePassData::image>());

    m_renderer->getDevice().flushDescriptorUpdates();
}

void AtmosphereScene::resize(int width, int height) {
    m_cameraController->onViewportResized(width, height);

    m_renderGraph->resize(m_renderer->getDevice(), m_renderer->getSwapChainExtent());
    m_renderer->setSceneImageView(&m_renderGraph->getImageView<&AtmospherePassData::image>());
}

void AtmosphereScene::update(const UpdateParams& updateParams) {
    m_cameraController->update(updateParams.dt);
    const auto& camParams = m_cameraController->getCameraParameters();
    const auto screenExtent = m_renderer->getSwapChainExtent();
    m_atmosphereParams.screenResolution = glm::vec2(screenExtent.width, screenExtent.height);
    m_atmosphereParams.VP = camParams.P * camParams.V;
    m_atmosphereParams.invVP = glm::inverse(m_atmosphereParams.VP);
    m_atmosphereParams.cameraPosition = m_cameraController->getCamera().getPosition();

    m_resourceContext->getRingBuffer("camera")
        ->updateStagingBufferFromStruct(camParams, updateParams.frameInFlightIdx);
    m_resourceContext->getRingBuffer("atmosphereBuffer")
        ->updateStagingBufferFromStruct(m_atmosphereParams, updateParams.frameInFlightIdx);
}

void AtmosphereScene::render(const FrameContext& frameContext) {
    frameContext.commandEncoder.insertBarrier(kFragmentUniformRead >> kTransferWrite);
    m_resourceContext->getRingBuffer("camera")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    m_resourceContext->getRingBuffer("atmosphereBuffer")->updateDeviceBuffer(frameContext.commandEncoder.getHandle());
    frameContext.commandEncoder.insertBarrier(kTransferWrite >> kFragmentUniformRead);

    m_renderGraph->execute(frameContext);
}

void AtmosphereScene::drawGui() {
    ImGui::Begin("Settings");
    if (ImGui::SliderFloat("Azimuth", &azimuth, 0.0f, 2.0f * glm::pi<float>())) {
        m_atmosphereParams.sunDirection.x = std::cos(azimuth) * std::cos(altitude);
        m_atmosphereParams.sunDirection.y = std::sin(altitude);
        m_atmosphereParams.sunDirection.z = std::sin(azimuth) * std::cos(altitude);
    }
    if (ImGui::SliderFloat("Altitude", &altitude, 0.0f, glm::pi<float>())) {
        m_atmosphereParams.sunDirection.x = std::cos(azimuth) * std::cos(altitude);
        m_atmosphereParams.sunDirection.y = std::sin(altitude);
        m_atmosphereParams.sunDirection.z = std::sin(azimuth) * std::cos(altitude);
    }
    ImGui::End();
}

void AtmosphereScene::setupInput() {
    m_connectionHandlers.emplace_back(m_window->keyPressed.subscribe([this](Key key, int) {
        switch (key) {
        case Key::F5:
            m_resourceContext->recreatePipelines();
            break;
        }
    }));
}
} // namespace crisp
