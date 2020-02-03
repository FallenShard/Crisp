#pragma once

#include <vector>
#include <CrispCore/Math/Headers.hpp>

namespace crisp
{
    struct TileFrustum
    {
        glm::vec4 frustumPlanes[4];
    };

    glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize);
    std::vector<TileFrustum> createTileFrusta(glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix);
}