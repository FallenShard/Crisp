#include <Crisp/Scenes/EnvironmentLightSampling.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace crisp {
namespace {

constexpr float luminance(const float* rgba) {
    return 0.2126f * rgba[0] + 0.7152f * rgba[1] + 0.0722f * rgba[2]; // NOLINT
}

} // namespace

Distribution2D createEnvironmentSamplingDistribution(
    const std::span<const float> rgbaPixels, const uint32_t width, const uint32_t height) {
    if (width == 0 || height == 0 || rgbaPixels.size() != static_cast<size_t>(width) * height * 4) {
        throw std::invalid_argument("Environment pixels must be a non-empty RGBA32F image");
    }

    std::vector<float> weights(static_cast<size_t>(width) * height);
    double totalWeight = 0.0;
    for (uint32_t y = 0; y < height; ++y) {
        const float sinTheta =
            std::sin(std::numbers::pi_v<float> * (static_cast<float>(y) + 0.5f) / static_cast<float>(height));
        for (uint32_t x = 0; x < width; ++x) {
            const float value = luminance(rgbaPixels.data() + (static_cast<size_t>(y) * width + x) * 4); // NOLINT
            const float weight = (std::isfinite(value) ? std::max(0.0f, value) : 0.0f) * sinTheta;
            weights[static_cast<size_t>(y) * width + x] = weight;
            totalWeight += weight;
        }
    }

    if (totalWeight == 0.0) {
        // A black map has no importance signal. A sine-weighted fallback remains well-defined in solid angle while
        // all returned radiance stays zero.
        for (uint32_t y = 0; y < height; ++y) {
            const float sinTheta =
                std::sin(std::numbers::pi_v<float> * (static_cast<float>(y) + 0.5f) / static_cast<float>(height));
            std::fill_n(weights.begin() + static_cast<size_t>(y) * width, width, sinTheta); // NOLINT
        }
    }

    return {weights, width, height}; // NOLINT
}

} // namespace crisp
