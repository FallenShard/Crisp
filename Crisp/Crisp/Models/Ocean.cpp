#include <Crisp/Models/Ocean.hpp>

#include <Crisp/Math/Headers.hpp>

#include <random>

namespace crisp
{
namespace
{
constexpr float g = 9.81f;

float calculatePhillipsSpectrum(const WindParameters& wind, const glm::vec2& k)
{
    const float kLen2 = glm::dot(k, k) + 0.000001f;
    const glm::vec2 kNorm = kLen2 == 0.0f ? glm::vec2(0.0f) : k / std::sqrtf(kLen2);

    const float A = 1000.0f;
    const float midterm = std::exp(-1.0f / (kLen2 * wind.Lw * wind.Lw)) / std::powf(kLen2, 2);
    const float kDotW = glm::dot(kNorm, wind.speedNorm);

    const float smallWaves = 1.0f;
    const float tail = std::exp(-kLen2 * smallWaves * smallWaves);

    return A * midterm * std::powf(kDotW, 2) * tail;
}
} // namespace

OceanParameters createOceanParameters(
    const int32_t patchGridSize,
    const float patchWorldSize,
    const float windX,
    const float windZ,
    const float A,
    const float l)
{
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

std::vector<glm::vec4> createOceanSpectrum(
    const uint32_t seed, const OceanParameters& oceanParams, const OceanSpectrumData spectrumData)
{
    WindParameters wind;
    wind.speed = {40, 40};
    wind.magnitude = std::sqrt(glm::dot(wind.speed, wind.speed));
    wind.speedNorm = wind.speed / wind.magnitude;
    wind.Lw = wind.magnitude * wind.magnitude / g;

    std::mt19937 gen{seed};
    std::normal_distribution<> distrib(0, 1);
    std::vector<glm::vec4> spectrum;
    for (int32_t i = 0; i < oceanParams.N; ++i)
    {
        for (int32_t j = 0; j < oceanParams.M; ++j)
        {

            const glm::vec2 eps0 = glm::vec2(distrib(gen), distrib(gen));
            const glm::vec2 eps1 = glm::vec2(distrib(gen), distrib(gen));
            if (spectrumData == OceanSpectrumData::UniformGaussian)
            {
                spectrum.emplace_back(eps0, eps1);
            }
            else
            {
                const glm::ivec2 idx = glm::ivec2(j, i) - glm::ivec2(oceanParams.N, oceanParams.M) / 2;
                const glm::vec2 k =
                    glm::vec2(idx) * 2.0f * glm::pi<float>() / glm::vec2(oceanParams.Lx, oceanParams.Lz);
                const glm::vec2 h0 = eps0 * 1.0f / std::sqrtf(2.0f) * std::sqrt(calculatePhillipsSpectrum(wind, k));
                glm::vec2 h0Star = eps1 * 1.0f / std::sqrtf(2.0f) * std::sqrt(calculatePhillipsSpectrum(wind, -k));
                h0Star.y = -h0Star.y;
                spectrum.emplace_back(h0, h0Star);
            }
        }
    }

    return spectrum;
}
} // namespace crisp
