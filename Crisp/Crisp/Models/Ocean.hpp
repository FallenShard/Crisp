#pragma once

#include <Crisp/Math/Headers.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace crisp {
// Must match OCEAN_CASCADE_COUNT in Shaders/Common/ocean.part.glsl.
inline constexpr uint32_t kOceanCascadeCount = 3;

// Must match the OCEAN_SPECTRUM_* defines in ocean-spectrum.comp.glsl.
enum class OceanSpectrumModel : uint8_t {
    Phillips = 0,
    PiersonMoskowitz = 1,
    Jonswap = 2,
};

inline constexpr std::array<const char*, 3> kOceanSpectrumModelNames{"Phillips (1986)", "Pierson-Moskowitz", "JONSWAP"};

// `A` is a pure gain on every model, but the models disagree by orders of magnitude about what a
// physical amplitude is, so each carries its own sensible starting point.
inline constexpr std::array<float, 3> kOceanSpectrumDefaultAmplitudes{0.001f, 1.0f, 1.0f};

// Spectrum controls shared by every cascade. The patch size is deliberately not here: it is the one
// thing that separates one cascade from the next.
struct OceanParameters {
    int32_t N;
    int32_t M;

    glm::vec2 windDirection;
    float windSpeed;
    float Lw;

    float A;
    float smallWaves;

    float time;

    OceanSpectrumModel spectrumModel;
    // Metres of open water the wind has blown across. Drives both the scale and the peak of the
    // fetch-limited models; Phillips has no notion of it.
    float fetch;
    // JONSWAP's peak enhancement, gamma. 1 collapses it onto Pierson-Moskowitz.
    float peakEnhancement;
    // s in the cos^2s(theta/2) spread. Higher is a narrower, more wind-aligned sea.
    float directionalSpread;
};

// One log-spaced slice of the spectrum. kMin is the previous cascade's Nyquist, so the bands neither
// overlap (energy counted twice reads as a too-rough sea) nor gap. See docs/ocean.md item 9.
struct OceanCascade {
    float patchWorldSize;
    float kMin;
    float kMax;
};

// Mirrors the push constant block in ocean-spectrum.comp.glsl.
struct OceanSpectrumPushConstants {
    int32_t N;
    int32_t M;
    float Lx;
    float Lz;

    glm::vec2 windDirection;
    float windSpeed;
    float Lw;

    float A;
    float smallWaves;

    float time;

    float kMin;
    float kMax;
    int32_t cascadeIndex;

    int32_t spectrumModel;
    float fetch;
    float peakEnhancement;
    float directionalSpread;
    float directionalSpreadNormalization;
};

static_assert(sizeof(OceanSpectrumPushConstants) == 76);

// Integrated second moments of one band. They move only when the spectrum does, so the CPU owns them
// rather than the shader re-deriving them per pixel.
struct OceanCascadeMoments {
    float heightVariance; // m^2.
    // Per axis. Widens the specular lobe once the band falls below a pixel; docs/ocean.md item 12.
    float slopeVariance;
};

OceanParameters createOceanParameters(int32_t patchGridSize, float windX, float windZ, float A, float l);

// Patch sizes must be descending, and want near-prime ratios: exact powers of two beat at the
// largest size and bring the tiling straight back.
std::array<OceanCascade, kOceanCascadeCount> createOceanCascades(
    const std::array<float, kOceanCascadeCount>& patchWorldSizes, int32_t patchGridSize);

OceanSpectrumPushConstants createOceanSpectrumPushConstants(
    const OceanParameters& oceanParams, const OceanCascade& cascade, int32_t cascadeIndex);

// Independent draws per cascade; correlated seeds would make the bands beat against each other.
std::vector<glm::vec2> createOceanSpectrum(uint32_t seed, const OceanParameters& oceanParams, uint32_t cascadeCount);

// Integrates N*M spectrum samples, so it is not something to call per frame. Every model is linear
// in `A`, so amplitude changes want a rescale rather than another integration.
OceanCascadeMoments computeCascadeMoments(const OceanParameters& oceanParams, const OceanCascade& cascade);

// Geometric mean of the band's wavelength range: the scale at which a sample spacing stops resolving
// it. Both the vertex grid and the pixel footprint are judged against this.
float computeBandWavelength(const OceanCascade& cascade);

// Makes the cos^2s(theta/2) spread integrate to one over all directions, so it redistributes the
// frequency spectrum's energy instead of adding to it.
float computeDirectionalSpreadNormalization(float directionalSpread);

std::vector<float> createTileableFoamNoise(uint32_t size, uint32_t seed, uint32_t octaves);
} // namespace crisp
