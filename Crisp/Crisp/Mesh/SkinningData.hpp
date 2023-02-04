#pragma once

#include <Crisp/Math/Headers.hpp>

#include <vector>

namespace crisp
{
struct SkinningData
{
    static constexpr uint32_t JointsPerVertex{4};

    std::vector<glm::vec4> weights;
    std::vector<glm::ivec4> indices;

    std::vector<glm::vec3> jointTranslations;
    std::vector<glm::quat> jointQuaternions;
    std::vector<glm::mat4> jointMatrices;
};
} // namespace crisp
