#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Math/Headers.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace crisp {
namespace {
constexpr float g = 9.81f;

// Must match calculateSpectrum in ocean-spectrum.comp.glsl, band limit and all. Nothing in the build
// checks the two agree; a drift here shows up as foam thresholds that no longer track the waves.
float phillipsDensity(const glm::vec2 kDir, const float kLen2, const OceanParameters& p) {
    const float expTerm = std::exp(-1.0f / (kLen2 * p.Lw * p.Lw)) / (kLen2 * kLen2);
    const float kDotW = glm::dot(kDir, p.windDirection);
    return expTerm * (kDotW * kDotW);
}

float dispersionJacobian(const float kLen) {
    return 0.5f * std::sqrt(g / kLen) / kLen;
}

float piersonMoskowitzDensity(const float kLen, const OceanParameters& p) {
    const float omega = std::sqrt(g * kLen);
    const float omegaPeak = 0.855f * g / std::max(p.windSpeed, 0.1f);
    const float omega5 = omega * omega * omega * omega * omega;
    const float shape = 0.0081f * g * g / omega5 * std::exp(-1.25f * std::pow(omegaPeak / omega, 4.0f));
    return shape * dispersionJacobian(kLen);
}

float jonswapDensity(const float kLen, const OceanParameters& p) {
    const float omega = std::sqrt(g * kLen);
    const float speed = std::max(p.windSpeed, 0.1f);
    const float fetch = std::max(p.fetch, 1.0f);
    const float dimensionlessFetch = g * fetch / (speed * speed);

    const float alpha = 0.076f * std::pow(dimensionlessFetch, -0.22f);
    const float omegaPeak = 22.0f * std::pow(g * g / (speed * fetch), 1.0f / 3.0f);

    const float sigma = omega <= omegaPeak ? 0.07f : 0.09f;
    const float relative = (omega - omegaPeak) / (sigma * omegaPeak);
    const float peak = std::pow(p.peakEnhancement, std::exp(-0.5f * relative * relative));

    const float omega5 = omega * omega * omega * omega * omega;
    const float shape = alpha * g * g / omega5 * std::exp(-1.25f * std::pow(omegaPeak / omega, 4.0f));
    return shape * peak * dispersionJacobian(kLen);
}

float directionalSpreadDensity(const glm::vec2 kDir, const OceanParameters& p) {
    const float cosHalfSquared = std::max(0.5f * (1.0f + glm::dot(kDir, p.windDirection)), 0.0f);
    return computeDirectionalSpreadNormalization(p.directionalSpread) * std::pow(cosHalfSquared, p.directionalSpread);
}

float evaluateSpectrum(const glm::vec2 k, const OceanParameters& p, const OceanCascade& cascade) {
    const float kLen2 = glm::dot(k, k);
    if (kLen2 == 0.0f) {
        return 0.0f;
    }

    const float kLen = std::sqrt(kLen2);
    if (kLen < cascade.kMin || kLen >= cascade.kMax) {
        return 0.0f;
    }

    const glm::vec2 kDir = k * glm::inversesqrt(kLen2);

    float density = 0.0f;
    switch (p.spectrumModel) {
    case OceanSpectrumModel::PiersonMoskowitz:
        density = piersonMoskowitzDensity(kLen, p) * directionalSpreadDensity(kDir, p);
        break;
    case OceanSpectrumModel::Jonswap:
        density = jonswapDensity(kLen, p) * directionalSpreadDensity(kDir, p);
        break;
    case OceanSpectrumModel::Phillips:
    default:
        density = phillipsDensity(kDir, kLen2, p);
        break;
    }

    return p.A * density * std::exp(-kLen2 * p.smallWaves * p.smallWaves);
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
        .time = 0.0f,
        .spectrumModel = OceanSpectrumModel::Phillips,
        // 100 km of open water and the standard gamma; only the fetch-limited models read these.
        .fetch = 100000.0f,
        .peakEnhancement = 3.3f,
        .directionalSpread = 4.0f};
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
        .cascadeIndex = cascadeIndex,
        .spectrumModel = static_cast<int32_t>(oceanParams.spectrumModel),
        .fetch = oceanParams.fetch,
        .peakEnhancement = oceanParams.peakEnhancement,
        .directionalSpread = oceanParams.directionalSpread,
        .directionalSpreadNormalization = computeDirectionalSpreadNormalization(oceanParams.directionalSpread)};
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
            const float density = evaluateSpectrum(k, oceanParams, cascade);
            heightSum += density;
            slopeSum += glm::dot(k, k) * density;
        }
    }

    // The 2 is the h0(k) + conj(h0(-k)) pair in the oscillation pass; both draws contribute variance.
    const float cellArea = 2.0f * dk * dk;
    // The k^2 weight sums both axes, and the spread is close enough to isotropic to split it evenly.
    return {.heightVariance = cellArea * heightSum, .slopeVariance = 0.5f * cellArea * slopeSum};
}

std::vector<float> createTileableFoamNoise(const uint32_t size, const uint32_t seed, const uint32_t octaves) {
    std::mt19937 gen{seed};
    std::uniform_real_distribution<float> distrib(0.0f, 1.0f);

    std::vector<float> noise(static_cast<size_t>(size) * size, 0.0f);
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;

    for (uint32_t octave = 0; octave < octaves; ++octave) {
        // The lattice period doubles per octave and divides `size`, which is what keeps each octave,
        // and therefore the sum, tileable.
        const uint32_t period = 4u << octave;
        std::vector<float> lattice(static_cast<size_t>(period) * period);
        for (float& value : lattice) {
            value = distrib(gen);
        }
        const auto latticeAt = [&](const uint32_t x, const uint32_t y) {
            return lattice[static_cast<size_t>(y % period) * period + (x % period)];
        };

        const float cellSize = static_cast<float>(size) / static_cast<float>(period);
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                const float fx = static_cast<float>(x) / cellSize;
                const float fy = static_cast<float>(y) / cellSize;
                const auto x0 = static_cast<uint32_t>(fx);
                const auto y0 = static_cast<uint32_t>(fy);
                const float tx = fx - static_cast<float>(x0);
                const float ty = fy - static_cast<float>(y0);
                const float sx = tx * tx * (3.0f - 2.0f * tx);
                const float sy = ty * ty * (3.0f - 2.0f * ty);

                const float top = std::lerp(latticeAt(x0, y0), latticeAt(x0 + 1, y0), sx);
                const float bottom = std::lerp(latticeAt(x0, y0 + 1), latticeAt(x0 + 1, y0 + 1), sx);
                noise[static_cast<size_t>(y) * size + x] += amplitude * std::lerp(top, bottom, sy);
            }
        }

        amplitudeSum += amplitude;
        amplitude *= 0.5f;
    }

    for (float& value : noise) {
        value /= amplitudeSum;
    }
    return noise;
}

float computeDirectionalSpreadNormalization(const float directionalSpread) {
    // The integral of cos^2s(theta/2) over all directions is 2*pi*C(2s,s)/4^s; through lgamma so it
    // stays finite for the large exponents a narrow spread asks for.
    const auto s = static_cast<double>(directionalSpread);
    const double logBinomial = std::lgamma(2.0 * s + 1.0) - 2.0 * std::lgamma(s + 1.0) - s * std::log(4.0);
    const double integral = 2.0 * std::numbers::pi * std::exp(logBinomial);
    return static_cast<float>(1.0 / integral);
}

float computeBandWavelength(const OceanCascade& cascade) {
    const float shortest = 2.0f * glm::pi<float>() / cascade.kMax;
    // The coarsest cascade has no kMin; its longest representable wave is the patch itself.
    const float longest = cascade.kMin > 0.0f ? 2.0f * glm::pi<float>() / cascade.kMin : cascade.patchWorldSize;
    return std::sqrt(shortest * longest);
}
} // namespace crisp
