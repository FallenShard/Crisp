#pragma once

#include <span>

#include <Crisp/Math/Distribution2D.hpp>

namespace crisp {

Distribution2D createEnvironmentSamplingDistribution(std::span<const float> rgbaPixels, uint32_t width, uint32_t height);

} // namespace crisp
