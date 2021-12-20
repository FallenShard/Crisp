#pragma once

#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
struct TransformPack
{
    glm::mat4 MVP;
    glm::mat4 MV;
    glm::mat4 M;
    glm::mat4 N;
};
} // namespace crisp