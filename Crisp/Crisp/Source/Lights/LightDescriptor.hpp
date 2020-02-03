#pragma once

#include <CrispCore/Math/Headers.hpp>

#include <algorithm>

namespace crisp
{
    struct LightDescriptor
    {
        glm::mat4 VP;
        glm::mat4 V;
        glm::mat4 P;
        glm::vec4 position;
        glm::vec3 spectrum;
        unsigned char padding[36];
    };

    struct ManyLightDescriptor
    {
        glm::vec3 position;
        float radius;
        glm::vec3 spectrum;
        float padding;

        inline void calculateRadius(float cutoff = 2.0f / 256.0f)
        {
            float maxIllum = std::max(spectrum.r, std::max(spectrum.g, spectrum.b));
            radius = std::sqrt(maxIllum / cutoff);
        }
    };
}