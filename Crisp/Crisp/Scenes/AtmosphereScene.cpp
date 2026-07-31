#include <Crisp/Scenes/AtmosphereScene.hpp>

#include <imgui.h>

namespace crisp {
namespace {
const auto logger = createLoggerMt("AtmosphereScene");

glm::vec3 computeSunDirection(const float azimuthDegrees, const float altitudeDegrees) {
    const float azimuth = glm::radians(azimuthDegrees);
    const float altitude = glm::radians(altitudeDegrees);
    const float cosE = std::cos(altitude);
    return {
        std::cos(azimuth) * cosE,
        std::sin(altitude),
        std::sin(azimuth) * cosE,
    };
}

// The default drag speed and formatting are useless for coefficients in the 1e-5 to 1e-1 range.
bool dragCoefficients(const char* label, glm::vec3& value, const float speed = 1e-4f) {
    return ImGui::DragFloat3(label, &value.x, speed, 0.0f, 1.0f, "%.6f");
}

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
    m_resourceContext->createUniformRingBuffer<CameraParameters>("camera");
    m_resourceContext->createUniformRingBuffer<AtmosphereParameters>("atmosphereBuffer");

    applyAtmosphereSettings();

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
    applyAtmosphereSettings();

    m_resourceContext->getRingBuffer("camera")->updateStagingBufferFromStruct(camParams, updateParams.frameInFlightIdx);
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

void AtmosphereScene::applyAtmosphereSettings() {
    m_atmosphereParams.sunDirection = computeSunDirection(m_settings.sunAzimuthDegrees, m_settings.sunElevationDegrees);
    m_atmosphereParams.sunIlluminance = glm::vec4(m_settings.sunColor * m_settings.sunIlluminanceScale, 1.0f);

    // The shaders evaluate exp(densityScale * altitude), so they want the negated reciprocal scale height.
    m_atmosphereParams.rayleighDensityScale = -1.0f / std::max(m_settings.rayleighScaleHeight, 1e-3f);
    m_atmosphereParams.mieDensityScale = -1.0f / std::max(m_settings.mieScaleHeight, 1e-3f);

    m_atmosphereParams.topRadius = m_atmosphereParams.bottomRadius + m_settings.atmosphereHeight;

    // Extinction is not independently authored: what is not scattered out of the beam has to be absorbed.
    m_atmosphereParams.mieExtinction = m_atmosphereParams.mieScattering + glm::vec3(m_atmosphereParams.mieAbsorption);
}

void AtmosphereScene::drawGui() {
    drawAtmosphereGui();
}

void AtmosphereScene::drawAtmosphereGui() { // NOLINT(readability-function-size)
    ImGui::SetNextWindowSize(ImVec2(440.0f, 700.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Atmosphere")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reset to Earth")) {
        // update() reinstates the camera-derived fields before anything is uploaded, so a wholesale reset here is
        // safe even though it also clears the view matrices.
        m_atmosphereParams = AtmosphereParameters{};
        m_settings = AtmosphereSettings{};
        CRISP_LOGI("Atmosphere parameters reset to Earth defaults.");
    }
    ImGui::Combo(
        "View",
        &m_atmosphereParams.debugViewMode,
        kDebugViewModeNames.data(),
        static_cast<int32_t>(kDebugViewModeNames.size()));
    drawTooltip("Replaces the final image with a full screen visualization of one of the intermediate LUTs.");
    ImGui::Separator();

    if (beginSection("Sun")) {
        ImGui::SliderFloat("Azimuth", &m_settings.sunAzimuthDegrees, 0.0f, 360.0f, "%.1f deg");
        ImGui::SliderFloat("Elevation", &m_settings.sunElevationDegrees, -90.0f, 90.0f, "%.1f deg");
        ImGui::ColorEdit3("Color", &m_settings.sunColor.x);
        ImGui::SliderFloat("Illuminance", &m_settings.sunIlluminanceScale, 0.0f, 20.0f, "%.2f");

        bool drawSunDisk = m_atmosphereParams.drawSunDisk != 0;
        if (ImGui::Checkbox("Draw sun disk", &drawSunDisk)) {
            m_atmosphereParams.drawSunDisk = drawSunDisk ? 1 : 0;
        }
        ImGui::SliderFloat("Angular diameter", &m_atmosphereParams.sunAngularDiameterDegrees, 0.05f, 5.0f, "%.3f deg");
        ImGui::DragFloat(
            "Disk luminance",
            &m_atmosphereParams.sunDiskLuminance,
            1000.0f,
            1.0f,
            1e9f,
            "%.0f",
            ImGuiSliderFlags_Logarithmic);

        const glm::vec3& dir = m_atmosphereParams.sunDirection;
        ImGui::TextDisabled("Direction: (%.3f, %.3f, %.3f)", dir.x, dir.y, dir.z); // NOLINT
    }

    if (beginSection("Planet")) {
        ImGui::DragFloat("Ground radius", &m_atmosphereParams.bottomRadius, 10.0f, 100.0f, 20000.0f, "%.0f km");
        ImGui::DragFloat("Atmosphere height", &m_settings.atmosphereHeight, 0.5f, 1.0f, 500.0f, "%.1f km");
        ImGui::ColorEdit3("Ground albedo", &m_atmosphereParams.groundAlbedo.x);
        drawTooltip("Feeds both the shaded planet surface and the bounced light term in the multiple scattering LUT.");

        bool renderGround = m_atmosphereParams.renderGround != 0;
        if (ImGui::Checkbox("Render ground", &renderGround)) {
            m_atmosphereParams.renderGround = renderGround ? 1 : 0;
        }
        drawTooltip(
            "Shades the planet surface where a view ray ends on it. With this off the surface stays black and "
            "the lower hemisphere is only the haze in front of it.");
        ImGui::TextDisabled("Top radius: %.1f km", m_atmosphereParams.topRadius); // NOLINT
    }

    if (beginSection("Rayleigh")) {
        dragCoefficients("Scattering##rayleigh", m_atmosphereParams.rayleighScattering);
        drawTooltip("Scattering coefficients in 1/km, per RGB wavelength band.");
        ImGui::SliderFloat("Scale height##rayleigh", &m_settings.rayleighScaleHeight, 0.1f, 30.0f, "%.2f km");
    }

    if (beginSection("Mie")) {
        dragCoefficients("Scattering##mie", m_atmosphereParams.mieScattering, 1e-5f);
        dragCoefficients(
            "Absorption##mie", reinterpret_cast<glm::vec3&>(m_atmosphereParams.mieAbsorption), 1e-5f); // NOLINT
        ImGui::SliderFloat("Scale height##mie", &m_settings.mieScaleHeight, 0.1f, 10.0f, "%.2f km");
        ImGui::SliderFloat("Phase g", &m_atmosphereParams.miePhaseG, -0.99f, 0.99f, "%.3f");
        drawTooltip(
            "Cornette-Shanks eccentricity. Positive values push the scattering lobe forward, which is what "
            "produces the bright halo around the sun.");
        const glm::vec3& extinction = m_atmosphereParams.mieExtinction;
        ImGui::TextDisabled("Extinction: (%.6f, %.6f, %.6f)", extinction.x, extinction.y, extinction.z); // NOLINT
    }

    if (beginSection("Ozone")) {
        dragCoefficients("Absorption##ozone", m_atmosphereParams.ozoneAbsorption, 1e-5f);
        ImGui::SliderFloat("Layer width", &m_atmosphereParams.absorptionDensity0LayerWidth, 0.0f, 60.0f, "%.1f km");
        drawTooltip(
            "Altitude at which the tent shaped ozone density profile switches from its rising half to its "
            "falling half.");
        ImGui::DragFloat("Lower linear", &m_atmosphereParams.absorptionDensity0LinearTerm, 1e-3f, -1.0f, 1.0f, "%.5f");
        ImGui::DragFloat(
            "Lower constant", &m_atmosphereParams.absorptionDensity0ConstantTerm, 1e-2f, -10.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Upper linear", &m_atmosphereParams.absorptionDensity1LinearTerm, 1e-3f, -1.0f, 1.0f, "%.5f");
        ImGui::DragFloat(
            "Upper constant", &m_atmosphereParams.absorptionDensity1ConstantTerm, 1e-2f, -10.0f, 10.0f, "%.4f");
    }

    if (beginSection("Sky view LUT")) {
        bool fastSkyEnabled = m_atmosphereParams.fastSkyEnabled != 0;
        if (ImGui::Checkbox("Fast sky", &fastSkyEnabled)) {
            m_atmosphereParams.fastSkyEnabled = fastSkyEnabled ? 1 : 0;
        }
        drawTooltip(
            "Resolves open sky with a single fetch from the sky view LUT instead of ray marching every pixel. "
            "Turn it off to compare against the reference full march.");
        ImGui::SliderFloat("Fallback altitude", &m_atmosphereParams.skyViewLutMaxAltitude, 0.0f, 100.0f, "%.1f km");
        drawTooltip(
            "Above this altitude the LUT is abandoned for a full march. Its vertical parameterization is built "
            "around the horizon as seen from the camera and loses the detail that matters once the ground is far "
            "below.");
    }

    if (beginSection("Ray marching")) {
        ImGui::SliderInt("Min samples", &m_atmosphereParams.minRayMarchingSamples, 1, 64);
        ImGui::SliderInt("Max samples", &m_atmosphereParams.maxRayMarchingSamples, 1, 64);
        drawTooltip("Sample count is interpolated between these two over the first 100 km of ray length.");
        m_atmosphereParams.maxRayMarchingSamples =
            std::max(m_atmosphereParams.maxRayMarchingSamples, m_atmosphereParams.minRayMarchingSamples);
        ImGui::SliderFloat("Multiple scattering", &m_atmosphereParams.multipleScatteringFactor, 0.0f, 2.0f, "%.2f");
        drawTooltip(
            "Scales the second-and-higher order scattering contribution. 1 is the physically motivated "
            "value; 0 isolates single scattering.");
        ImGui::TextDisabled( // NOLINT
            "Transmittance LUT: %ux%u", kTransmittanceLutWidth, kTransmittanceLutHeight);
        ImGui::TextDisabled( // NOLINT
            "Multiple scattering LUT: %ux%u",
            kMultiScatteringLutResolution,
            kMultiScatteringLutResolution);
        ImGui::TextDisabled("Sky view LUT: %ux%u", kSkyViewLutWidth, kSkyViewLutHeight); // NOLINT
        ImGui::TextDisabled(                                                                // NOLINT
            "Aerial perspective LUT: %ux%ux%u (%.1f km/slice)",
            kCameraVolumeLutWidth,
            kCameraVolumeLutHeight,
            kCameraVolumeLutSliceCount,
            kCameraVolumeKmPerSlice);
    }

    if (beginSection("Presentation")) {
        ImGui::SliderFloat(
            "Exposure", &m_atmosphereParams.exposure, 0.01f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        drawTooltip(
            "Plain multiplier applied before the sRGB encode in the present pass; there is no tone mapping "
            "in this scene yet.");
    }

    if (beginSection("Camera")) {
        if (ImGui::SliderFloat("Speed", &m_cameraSpeed, 0.01f, 100.0f, "%.2f km/s", ImGuiSliderFlags_Logarithmic)) {
            m_cameraController->setSpeed(m_cameraSpeed);
        }
        float fovY = m_cameraController->getCamera().getVerticalFov();
        if (ImGui::SliderFloat("Vertical FOV", &fovY, 5.0f, 90.0f, "%.1f deg")) {
            m_cameraController->setFovY(fovY);
        }
        const glm::vec3& pos = m_atmosphereParams.cameraPosition;
        ImGui::TextDisabled("Position: (%.3f, %.3f, %.3f) km", pos.x, pos.y, pos.z); // NOLINT
        ImGui::TextDisabled("Altitude: %.3f km", pos.y);                             // NOLINT
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
