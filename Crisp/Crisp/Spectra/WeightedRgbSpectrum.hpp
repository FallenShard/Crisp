#pragma once

#include <Crisp/Spectra/RgbSpectrum.hpp>

namespace crisp {
struct WeightedRgbSpectrum : public glm::vec4 {
    WeightedRgbSpectrum(float value = 0.f)
        : glm::vec4(value, value, value, value) {}

    WeightedRgbSpectrum(const RgbSpectrum& rgb)
        : glm::vec4(rgb.r, rgb.g, rgb.b, 1.f) {}

    WeightedRgbSpectrum(float red, float green, float blue, float weight)
        : glm::vec4(red, green, blue, weight) {}

    WeightedRgbSpectrum(glm::vec4&& vec)
        : glm::vec4(vec) {}

    RgbSpectrum toRgb() const {
        if (w == 0.0) {
            return RgbSpectrum(0.f);
        }

        return RgbSpectrum(glm::vec3(r, g, b) / w);
    }
};
} // namespace crisp