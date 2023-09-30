#pragma once

#include <Crisp/Math/Headers.hpp>

#include <vector>

namespace crisp {
struct WindParameters {
    glm::vec2 speed;
    glm::vec2 speedNorm;
    float magnitude;
    float Lw;
};

enum class OceanSpectrumData {
    UniformGaussian,
    Phillips
};

struct OceanParameters {
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
    float pad0;
};

OceanParameters createOceanParameters(
    int32_t patchGridSize, float patchWorldSize, float windX, float windZ, float A, float l);

std::vector<glm::vec4> createOceanSpectrum(
    uint32_t seed,
    const OceanParameters& oceanParams,
    OceanSpectrumData spectrumData = OceanSpectrumData::UniformGaussian);
} // namespace crisp
