#pragma once

#include <Crisp/Renderer/RenderNode.hpp>

#include <CrispCore/Math/Headers.hpp>
#include <CrispCore/RobinHood.hpp>

#include <string>

namespace crisp
{
class RenderGraph;
class Renderer;
class ResourceContext;

static constexpr float EarthMieScaleHeight = 1.2f;
static constexpr float EarthRayleighScaleHeight = 8.0f;

struct AtmosphereParametersBuffer
{
    glm::mat4 VP = glm::transpose(glm::mat4{
        -0.856325269,
        0.00,
        0.00,
        0.00,
        0.00,
        0.00,
        1.52235603,
        -0.761178017,
        0.00,
        1.00000501,
        0.00,
        0.900004506,
        0.00,
        1.00,
        0.00,
        1.00,
    });
    glm::mat4 invVP = glm::transpose(glm::mat4{
        -1.16778052,
        0.00,
        0.00,
        0.00,
        0.00,
        0.00,
        9.99994850,
        -8.99999905,
        0.00,
        0.656876564,
        -4.99997425,
        4.99999952,
        0.00,
        0.00,
        -9.99994850,
        9.99999905,
    });

    // Rayleigh scattering coefficients + exponential distribution scale in the atmosphere
    glm::vec3 rayleighScattering{ 0.005802f, 0.013558f, 0.033100f };
    float rayleighDensityScale{ -1.0f / EarthRayleighScaleHeight };

    // Mie scattering coefficients + exponential distribution scale in the atmosphere
    glm::vec3 mieScattering{ 0.003996f, 0.003996f, 0.003996f };
    float mieDensityScale{ -1.0f / EarthMieScaleHeight };

    // Mie extinction coefficients + phase function excentricity
    glm::vec3 mieExtinctionAndPhase = glm::vec3(0.004440f);
    float miePhaseG{ 0.80f };

    // Mie absorption coefficients
    glm::vec4 mieAbsorption = glm::vec4(0.00044f);

    // Ozone absorption and layer width
    glm::vec3 ozoneAbsorption = glm::vec3(0.000650f, 0.001881f, 0.000085f);

    // Another medium type in the atmosphere
    float absorptionDensity0LayerWidth = 25.0f;

    float absorptionDensity0ConstantTerm = -2.0f / 3.0f;
    float absorptionDensity0LinearTerm = 1.0f / 15.0f;
    float absorptionDensity1ConstantTerm = 8.0f / 3.0f;
    float absorptionDensity1LinearTerm = -1.0f / 15.0f;

    // The albedo of the ground.
    glm::vec3 groundAlbedo = glm::vec3(0.0f);

    // Radius of the planet (center to ground) in km.
    float bottomRadius = 6360.0f;

    // glm::vec3 sunDirection = { 0.00, 0.90045, 0.43497 };
    glm::vec3 sunDirection = { 0.00, 0.43497, -0.90045 };

    // Maximum considered atmosphere height (center to atmosphere top) in km.
    float topRadius = 6460.0f;

    glm::vec4 sunIlluminance = glm::vec4(1.0f);

    // glm::vec3 cameraPosition = { 0.00, -1.00, 0.50 };
    glm::vec3 cameraPosition = { 0.00, 0.5, 1.0 };
    int32_t multiScatteringLutResolution = 32;

    glm::vec2 screenResolution;
    int32_t transmittanceLutWidth = 256;
    int32_t transmittanceLutHeight = 64;

    int minRayMarchingSamples = 4;
    int maxRayMarchingSamples = 14;
};

robin_hood::unordered_flat_map<std::string, std::unique_ptr<RenderNode>> addAtmosphereRenderPasses(
    RenderGraph& renderGraph, Renderer& renderer, ResourceContext& resourceContext, const std::string& dependentPass);

} // namespace crisp