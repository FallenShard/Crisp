#pragma once

#include <algorithm>

#include <Crisp/Math/Headers.hpp>

namespace crisp {
struct LightDescriptor {
    glm::mat4 V;
    glm::mat4 P;
    glm::mat4 VP;
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 spectrum;
    glm::vec4 params;
};

struct ManyLightDescriptor {
    glm::vec3 position;
    float radius;
    glm::vec3 spectrum;
    float padding;

    void calculateRadius(float cutoff = 2.0f / 256.0f) {
        float maxIllum = std::max({spectrum.r, spectrum.g, spectrum.b});
        radius = std::sqrt(maxIllum / cutoff);
    }
};
} // namespace crisp