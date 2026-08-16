#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Math/Headers.hpp>

#include <cmath>
#include <random>

namespace crisp {
namespace {
constexpr float g = 9.81f;

// Must match calculatePhillipsSpectrum in ocean-spectrum.comp.glsl.
float evaluatePhillipsSpectrum(const glm::vec2 k, const OceanParameters& p) {
    const float kLen2 = glm::dot(k, k);
    if (kLen2 == 0.0f) {
        return 0.0f;
    }

    const float kDotW = glm::dot(k * glm::inversesqrt(kLen2), p.windDirection);
    return p.A * std::exp(-1.0f / (kLen2 * p.Lw * p.Lw)) / (kLen2 * kLen2) * (kDotW * kDotW) *
           std::exp(-kLen2 * p.smallWaves * p.smallWaves);
}
} // namespace

OceanParameters createOceanParameters(
    const int32_t patchGridSize,
    const float patchWorldSize,
    const float windX,
    const float windZ,
    const float A,
    const float l) {
    const glm::vec2 windVelocity{windX, windZ};
    return {
        .N = patchGridSize,
        .M = patchGridSize,
        .Lx = patchWorldSize,
        .Lz = patchWorldSize,
        .windDirection = glm::normalize(windVelocity),
        .windSpeed = glm::length(windVelocity),
        .Lw = glm::dot(windVelocity, windVelocity) / g,
        .A = A,
        .smallWaves = l,
        .time = 0.0f};
}

std::vector<glm::vec2> createOceanSpectrum(const uint32_t seed, const OceanParameters& oceanParams) {
    std::mt19937 gen{seed};
    std::normal_distribution<float> distrib(0.0f, 1.0f);

    std::vector<glm::vec2> spectrum;
    spectrum.reserve(static_cast<size_t>(oceanParams.N) * static_cast<size_t>(oceanParams.M));
    for (int32_t i = 0; i < oceanParams.N; ++i) {
        for (int32_t j = 0; j < oceanParams.M; ++j) {
            spectrum.emplace_back(distrib(gen), distrib(gen));
        }
    }

    return spectrum;
}

float computeRmsWaveHeight(const OceanParameters& oceanParams) {
    const float dkx = 2.0f * glm::pi<float>() / oceanParams.Lx;
    const float dkz = 2.0f * glm::pi<float>() / oceanParams.Lz;

    const int32_t halfN = oceanParams.N / 2;
    const int32_t halfM = oceanParams.M / 2;
    float spectrumSum = 0.0f;
    for (int32_t i = 0; i < oceanParams.N; ++i) {
        for (int32_t j = 0; j < oceanParams.M; ++j) {
            const glm::vec2 k{static_cast<float>(i - halfN) * dkx, static_cast<float>(j - halfM) * dkz};
            spectrumSum += evaluatePhillipsSpectrum(k, oceanParams);
        }
    }

    // The 2 is the h0(k) + conj(h0(-k)) pair in the oscillation pass; both draws contribute variance.
    return std::sqrt(2.0f * dkx * dkz * spectrumSum);
}
} // namespace crisp
