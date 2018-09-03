#pragma once

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    struct LightDescriptorData
    {
        glm::mat4 VP;
        glm::mat4 V;
        glm::mat4 P;
        glm::vec4 position;
        glm::vec3 spectrum;
    };
}