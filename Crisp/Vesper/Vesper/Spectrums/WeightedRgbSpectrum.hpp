#pragma once

#include <algorithm>
#include <glm/glm.hpp>

#include "RgbSpectrum.hpp"

namespace vesper
{
    struct WeightedRgbSpectrum : public glm::vec4
    {
        WeightedRgbSpectrum(float value = 0.f)
            : glm::vec4(value, value, value, value)
        {
        }

        WeightedRgbSpectrum(const RgbSpectrum& rgb)
            : glm::vec4(rgb.r, rgb.g, rgb.b, 1.f)
        {
        }

        WeightedRgbSpectrum(float red, float green, float blue, float weight)
            : glm::vec4(red, green, blue, weight)
        {
        }

        WeightedRgbSpectrum(glm::vec4&& vec)
            : glm::vec4(vec)
        {
        }

        RgbSpectrum toRgb() const
        {
            if (w != 0)
                return glm::vec3(r, g, b) / w;
            else
                return RgbSpectrum(0.f);
        }
    };
}