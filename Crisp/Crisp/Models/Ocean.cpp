#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Math/Headers.hpp>

#include <random>

namespace crisp {
namespace {
constexpr float g = 9.81f;
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
} // namespace crisp
