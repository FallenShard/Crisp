#pragma once

#include <glm/glm.hpp>

namespace crisp
{
    struct Transforms
    {
        glm::mat4 MVP;
        glm::mat4 MV;
        glm::mat4 M;
        glm::mat4 N;
    };
}