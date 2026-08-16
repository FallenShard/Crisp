#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Math/Headers.hpp>

#include <cmath>
#include <random>

namespace crisp {
namespace {
constexpr float g = 9.81f;

// Must match calculatePhillipsSpectrum in ocean-spectrum.comp.glsl, band limit included.
float evaluatePhillipsSpectrum(const glm::vec2 k, const OceanParameters& p, const OceanCascade& cascade) {
    const float kLen2 = glm::dot(k, k);
    if (kLen2 == 0.0f) {
        return 0.0f;
    }

    const float kLen = std::sqrt(kLen2);
    if (kLen < cascade.kMin || kLen >= cascade.kMax) {
        return 0.0f;
    }

    const float kDotW = glm::dot(k * glm::inversesqrt(kLen2), p.windDirection);
    return p.A * std::exp(-1.0f / (kLen2 * p.Lw * p.Lw)) / (kLen2 * kLen2) * (kDotW * kDotW) *
           std::exp(-kLen2 * p.smallWaves * p.smallWaves);
}
} // namespace

OceanParameters createOceanParameters(
    const int32_t patchGridSize, const float windX, const float windZ, const float A, const float l) {
    const glm::vec2 windVelocity{windX, windZ};
    return {
        .N = patchGridSize,
        .M = patchGridSize,
        .windDirection = glm::normalize(windVelocity),
        .windSpeed = glm::length(windVelocity),
        .Lw = glm::dot(windVelocity, windVelocity) / g,
        .A = A,
        .smallWaves = l,
        .time = 0.0f};
}

std::array<OceanCascade, kOceanCascadeCount> createOceanCascades(
    const std::array<float, kOceanCascadeCount>& patchWorldSizes, const int32_t patchGridSize) {
    std::array<OceanCascade, kOceanCascadeCount> cascades{};
    float kMin = 0.0f;
    for (uint32_t i = 0; i < kOceanCascadeCount; ++i) {
        // Circular Nyquist: what falls past it is the next, finer cascade's to carry.
        const float kNyquist = glm::pi<float>() * static_cast<float>(patchGridSize) / patchWorldSizes[i];
        cascades[i] = {.patchWorldSize = patchWorldSizes[i], .kMin = kMin, .kMax = kNyquist};
        kMin = kNyquist;
    }
    return cascades;
}

OceanSpectrumPushConstants createOceanSpectrumPushConstants(
    const OceanParameters& oceanParams, const OceanCascade& cascade, const int32_t cascadeIndex) {
    return {
        .N = oceanParams.N,
        .M = oceanParams.M,
        .Lx = cascade.patchWorldSize,
        .Lz = cascade.patchWorldSize,
        .windDirection = oceanParams.windDirection,
        .windSpeed = oceanParams.windSpeed,
        .Lw = oceanParams.Lw,
        .A = oceanParams.A,
        .smallWaves = oceanParams.smallWaves,
        .time = oceanParams.time,
        .kMin = cascade.kMin,
        .kMax = cascade.kMax,
        .cascadeIndex = cascadeIndex};
}

std::vector<glm::vec2> createOceanSpectrum(
    const uint32_t seed, const OceanParameters& oceanParams, const uint32_t cascadeCount) {
    std::mt19937 gen{seed};
    std::normal_distribution<float> distrib(0.0f, 1.0f);

    const size_t drawCount =
        static_cast<size_t>(oceanParams.N) * static_cast<size_t>(oceanParams.M) * static_cast<size_t>(cascadeCount);
    std::vector<glm::vec2> spectrum;
    spectrum.reserve(drawCount);
    for (size_t i = 0; i < drawCount; ++i) {
        spectrum.emplace_back(distrib(gen), distrib(gen));
    }

    return spectrum;
}

OceanCascadeMoments computeCascadeMoments(const OceanParameters& oceanParams, const OceanCascade& cascade) {
    const float dk = 2.0f * glm::pi<float>() / cascade.patchWorldSize;

    const int32_t halfN = oceanParams.N / 2;
    const int32_t halfM = oceanParams.M / 2;
    float heightSum = 0.0f;
    float slopeSum = 0.0f;
    for (int32_t i = 0; i < oceanParams.N; ++i) {
        for (int32_t j = 0; j < oceanParams.M; ++j) {
            const glm::vec2 k{static_cast<float>(i - halfN) * dk, static_cast<float>(j - halfM) * dk};
            const float density = evaluatePhillipsSpectrum(k, oceanParams, cascade);
            heightSum += density;
            slopeSum += glm::dot(k, k) * density;
        }
    }

    // The 2 is the h0(k) + conj(h0(-k)) pair in the oscillation pass; both draws contribute variance.
    const float cellArea = 2.0f * dk * dk;
    // The k^2 weight sums both axes, and the spread is close enough to isotropic to split it evenly.
    return {.heightVariance = cellArea * heightSum, .slopeVariance = 0.5f * cellArea * slopeSum};
}

float computeBandWavelength(const OceanCascade& cascade) {
    const float shortest = 2.0f * glm::pi<float>() / cascade.kMax;
    // The coarsest cascade has no kMin; its longest representable wave is the patch itself.
    const float longest = cascade.kMin > 0.0f ? 2.0f * glm::pi<float>() / cascade.kMin : cascade.patchWorldSize;
    return std::sqrt(shortest * longest);
}
} // namespace crisp
