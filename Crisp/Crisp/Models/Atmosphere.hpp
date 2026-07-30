#pragma once

#include <array>
#include <cstddef>

#include <Crisp/Core/HashMap.hpp>
#include <Crisp/Math/Headers.hpp>

#include <Crisp/Renderer/RenderGraph/RenderGraph.hpp>

namespace crisp {
class Renderer;
class ResourceContext;

// Scale heights of the exponentially distributed media, in km.
constexpr float kEarthMieScaleHeight = 1.2f;
constexpr float kEarthRayleighScaleHeight = 8.0f;

// Mirrors the AtmosphereParams struct in Shaders/Common/atmosphere.part.glsl.
struct AtmosphereParameters {
    glm::mat4 VP{1.0f};
    glm::mat4 invVP{1.0f};

    // Rayleigh scattering coefficients (1/km) + exponential density falloff, i.e. -1 / scaleHeight.
    glm::vec3 rayleighScattering{0.005802f, 0.013558f, 0.033100f};
    float rayleighDensityScale{-1.0f / kEarthRayleighScaleHeight};

    // Mie scattering coefficients (1/km) + exponential density falloff, i.e. -1 / scaleHeight.
    glm::vec3 mieScattering{0.003996f, 0.003996f, 0.003996f};
    float mieDensityScale{-1.0f / kEarthMieScaleHeight};

    // Mie extinction (scattering + absorption) + Cornette-Shanks phase function eccentricity.
    glm::vec3 mieExtinction{0.004440f};
    float miePhaseG{0.80f};

    // Mie absorption coefficients (1/km).
    glm::vec4 mieAbsorption{0.000444f};

    // Ozone absorption coefficients (1/km).
    glm::vec3 ozoneAbsorption{0.000650f, 0.001881f, 0.000085f};

    // Altitude in km at which the tent shaped ozone profile switches from its rising to its falling half.
    float absorptionDensity0LayerWidth{25.0f};

    float absorptionDensity0ConstantTerm{-2.0f / 3.0f};
    float absorptionDensity0LinearTerm{1.0f / 15.0f};
    float absorptionDensity1ConstantTerm{8.0f / 3.0f};
    float absorptionDensity1LinearTerm{-1.0f / 15.0f};

    // The albedo of the ground.
    // Earth's mean albedo. Feeds both the shaded surface and the bounced light term in the multiple scattering LUT.
    glm::vec3 groundAlbedo{0.3f};

    // Radius of the planet (center to ground) in km.
    float bottomRadius{6360.0f};

    // Direction towards the sun.
    glm::vec3 sunDirection{0.0f, 0.43497f, -0.90045f};

    // Maximum considered atmosphere height (center to atmosphere top) in km.
    float topRadius{6460.0f};

    glm::vec4 sunIlluminance{1.0f};

    // Camera position in km, relative to the ground directly below it.
    glm::vec3 cameraPosition{0.0f, 0.5f, 1.0f};
    int32_t multiScatteringLutResolution{32};

    glm::vec2 screenResolution{};
    int32_t transmittanceLutWidth{256};
    int32_t transmittanceLutHeight{64};

    int32_t minRayMarchingSamples{4};
    int32_t maxRayMarchingSamples{14};

    float exposure{5.0f};

    // Earth's sun subtends roughly 0.545 degrees.
    float sunAngularDiameterDegrees{0.545f};

    // Same units as the ray marched sky, which sits around 0.05 here. Sized from the real sun to sky luminance
    // ratio of roughly 2e5, rather than the 1e6 the reference uses on its differently scaled sky.
    float sunDiskLuminance{1e4f};

    // 1 is the physically motivated value; 0 isolates single scattering.
    float multipleScatteringFactor{1.0f};

    // Indexes kDebugViewModeNames.
    int32_t debugViewMode{0};

    int32_t drawSunDisk{1};

    // Off leaves the reference full march to compare against.
    int32_t fastSkyEnabled{1};

    // Off leaves the surface black behind the haze, as in the reference implementation.
    int32_t renderGround{1};

    // In km. The LUT's axes are built around the horizon as seen from the camera, so it stops being usable once
    // the ground is far below.
    float skyViewLutMaxAltitude{4.0f};

    // Keeps the block a multiple of 16 bytes; claim it when the next flag lands.
    float pad0{0.0f};
};

inline constexpr std::array<const char*, 5> kDebugViewModeNames{
    "Atmosphere",
    "Transmittance LUT",
    "Multiple Scattering LUT",
    "Sky View LUT",
    "Aerial Perspective LUT",
};

struct AtmospherePassData {
    RenderGraphResourceHandle image;
};

void addAtmosphereRenderPasses(rg::RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext);

} // namespace crisp
