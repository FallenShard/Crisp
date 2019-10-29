#pragma once

#include <CrispCore/Math/Headers.hpp>

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
}