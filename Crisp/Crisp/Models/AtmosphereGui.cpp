#include <Crisp/Models/AtmosphereGui.hpp>

#include <algorithm>

#include <imgui.h>

namespace crisp {
namespace {
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

void drawAtmosphereGuiContents(
    AtmosphereSettings& settings, AtmosphereParameters& params, const AtmosphereGuiOptions& options) {
    if (ImGui::Button("Reset to Earth")) {
        // The camera-derived fields are reinstated every frame before anything is uploaded, so a wholesale reset
        // is safe here even though it also clears the view matrices.
        params = AtmosphereParameters{};
        settings = AtmosphereSettings{};
    }

    if (options.showDebugViewMode) {
        ImGui::Combo(
            "View", &params.debugViewMode, kDebugViewModeNames.data(), static_cast<int32_t>(kDebugViewModeNames.size()));
        drawTooltip("Replaces the final image with a full screen visualization of one of the intermediate LUTs.");
    }
    ImGui::Separator();

    if (beginSection("Sun")) {
        ImGui::SliderFloat("Azimuth", &settings.sunAzimuthDegrees, 0.0f, 360.0f, "%.1f deg");
        ImGui::SliderFloat("Elevation", &settings.sunElevationDegrees, -90.0f, 90.0f, "%.1f deg");
        ImGui::ColorEdit3("Color", &settings.sunColor.x);
        ImGui::SliderFloat("Irradiance", &settings.sunIrradianceScale, 0.0f, 20.0f, "%.2f");
        drawTooltip(
            "Radiometric convention: this is the irradiance entering the atmosphere, and every shader that "
            "consumes it returns radiance. Values are normalized rather than physical.");

        if (options.showSunDisk) {
            bool drawSunDisk = params.drawSunDisk != 0;
            if (ImGui::Checkbox("Draw sun disk", &drawSunDisk)) {
                params.drawSunDisk = drawSunDisk ? 1 : 0;
            }
            ImGui::SliderFloat("Angular diameter", &params.sunAngularDiameterDegrees, 0.05f, 5.0f, "%.3f deg");
            ImGui::DragFloat(
                "Disk radiance", &params.sunDiskRadiance, 1000.0f, 1.0f, 1e9f, "%.0f", ImGuiSliderFlags_Logarithmic);
        }

        const glm::vec3& dir = params.sunDirection;
        ImGui::TextDisabled("Direction: (%.3f, %.3f, %.3f)", dir.x, dir.y, dir.z); // NOLINT
    }

    if (beginSection("Planet")) {
        ImGui::DragFloat("Ground radius", &params.bottomRadius, 10.0f, 100.0f, 20000.0f, "%.0f km");
        ImGui::DragFloat("Atmosphere height", &settings.atmosphereHeight, 0.5f, 1.0f, 500.0f, "%.1f km");
        ImGui::ColorEdit3("Ground albedo", &params.groundAlbedo.x);
        drawTooltip("Feeds both the shaded planet surface and the bounced light term in the multiple scattering LUT.");

        bool renderGround = params.renderGround != 0;
        if (ImGui::Checkbox("Render ground", &renderGround)) {
            params.renderGround = renderGround ? 1 : 0;
        }
        drawTooltip(
            "Shades the planet surface where a view ray ends on it. With this off the surface stays black and "
            "the lower hemisphere is only the haze in front of it.");
        ImGui::TextDisabled("Top radius: %.1f km", params.topRadius); // NOLINT
    }

    if (beginSection("Rayleigh")) {
        dragCoefficients("Scattering##rayleigh", params.rayleighScattering);
        drawTooltip("Scattering coefficients in 1/km, per RGB wavelength band.");
        ImGui::SliderFloat("Scale height##rayleigh", &settings.rayleighScaleHeight, 0.1f, 30.0f, "%.2f km");
    }

    if (beginSection("Mie")) {
        dragCoefficients("Scattering##mie", params.mieScattering, 1e-5f);
        dragCoefficients("Absorption##mie", reinterpret_cast<glm::vec3&>(params.mieAbsorption), 1e-5f); // NOLINT
        ImGui::SliderFloat("Scale height##mie", &settings.mieScaleHeight, 0.1f, 10.0f, "%.2f km");
        ImGui::SliderFloat("Phase g", &params.miePhaseG, -0.99f, 0.99f, "%.3f");
        drawTooltip(
            "Cornette-Shanks eccentricity. Positive values push the scattering lobe forward, which is what "
            "produces the bright halo around the sun.");
        const glm::vec3& extinction = params.mieExtinction;
        ImGui::TextDisabled("Extinction: (%.6f, %.6f, %.6f)", extinction.x, extinction.y, extinction.z); // NOLINT
    }

    if (beginSection("Ozone")) {
        dragCoefficients("Absorption##ozone", params.ozoneAbsorption, 1e-5f);
        ImGui::SliderFloat("Layer width", &params.absorptionDensity0LayerWidth, 0.0f, 60.0f, "%.1f km");
        drawTooltip(
            "Altitude at which the tent shaped ozone density profile switches from its rising half to its "
            "falling half.");
        ImGui::DragFloat("Lower linear", &params.absorptionDensity0LinearTerm, 1e-3f, -1.0f, 1.0f, "%.5f");
        ImGui::DragFloat("Lower constant", &params.absorptionDensity0ConstantTerm, 1e-2f, -10.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Upper linear", &params.absorptionDensity1LinearTerm, 1e-3f, -1.0f, 1.0f, "%.5f");
        ImGui::DragFloat("Upper constant", &params.absorptionDensity1ConstantTerm, 1e-2f, -10.0f, 10.0f, "%.4f");
    }

    if (beginSection("Sky view LUT")) {
        if (options.showRayMarchingDebug) {
            bool fastSkyEnabled = params.fastSkyEnabled != 0;
            if (ImGui::Checkbox("Fast sky", &fastSkyEnabled)) {
                params.fastSkyEnabled = fastSkyEnabled ? 1 : 0;
            }
            drawTooltip(
                "Resolves open sky with a single fetch from the sky view LUT instead of ray marching every pixel. "
                "Turn it off to compare against the reference full march.");
        }
        ImGui::SliderFloat("Fallback altitude", &params.skyViewLutMaxAltitude, 0.0f, 100.0f, "%.1f km");
        drawTooltip(
            "Above this altitude the LUT is abandoned for a full march. Its vertical parameterization is built "
            "around the horizon as seen from the camera and loses the detail that matters once the ground is far "
            "below.");
    }

    if (beginSection("Aerial perspective")) {
        if (options.showRayMarchingDebug) {
            bool fastAerialPerspective = params.fastAerialPerspectiveEnabled != 0;
            if (ImGui::Checkbox("Fast aerial perspective", &fastAerialPerspective)) {
                params.fastAerialPerspectiveEnabled = fastAerialPerspective ? 1 : 0;
            }
            drawTooltip(
                "Resolves shaded geometry from the froxel volume with a single fetch instead of marching it. Inert "
                "until a depth pre-pass writes the view depth texture, since every pixel currently reads as open "
                "sky.");
        }
        ImGui::TextDisabled("Volume range: %.0f km", kCameraVolumeMaxDistance); // NOLINT
    }

    if (beginSection("Ray marching")) {
        ImGui::SliderInt("Min samples", &params.minRayMarchingSamples, 1, 64);
        ImGui::SliderInt("Max samples", &params.maxRayMarchingSamples, 1, 64);
        drawTooltip("Sample count is interpolated between these two over the first 100 km of ray length.");
        params.maxRayMarchingSamples = std::max(params.maxRayMarchingSamples, params.minRayMarchingSamples);
        ImGui::SliderFloat("Multiple scattering", &params.multipleScatteringFactor, 0.0f, 2.0f, "%.2f");
        drawTooltip(
            "Scales the second-and-higher order scattering contribution. 1 is the physically motivated "
            "value; 0 isolates single scattering.");
        ImGui::TextDisabled("Transmittance LUT: %ux%u", kTransmittanceLutWidth, kTransmittanceLutHeight); // NOLINT
        ImGui::TextDisabled(                                                                              // NOLINT
            "Multiple scattering LUT: %ux%u",
            kMultiScatteringLutResolution,
            kMultiScatteringLutResolution);
        ImGui::TextDisabled("Sky view LUT: %ux%u", kSkyViewLutWidth, kSkyViewLutHeight); // NOLINT
        ImGui::TextDisabled(                                                             // NOLINT
            "Aerial perspective LUT: %ux%ux%u (%.1f km/slice)",
            kCameraVolumeLutWidth,
            kCameraVolumeLutHeight,
            kCameraVolumeLutSliceCount,
            kCameraVolumeKmPerSlice);
    }
}

} // namespace crisp
