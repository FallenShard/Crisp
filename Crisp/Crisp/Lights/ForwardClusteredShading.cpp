#include <Crisp/Lights/ForwardClusteredShading.hpp>

namespace crisp
{
    namespace
    {
        struct Tile
        {
            glm::vec3 screenSpacePoints[4];
            glm::vec3 viewSpacePoints[4];
        };

        glm::vec4 computePlaneFromSpan(const glm::vec3& a, const glm::vec3& b)
        {
            glm::vec3 n = glm::normalize(glm::cross(a, b));
            return glm::vec4(n, glm::dot(n, a));
        }
    }

    glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize)
    {
        return (glm::ivec2(screenSize) - glm::ivec2(1)) / tileSize + glm::ivec2(1);
    }

    std::vector<TileFrustum> createTileFrusta(glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix)
    {
        glm::ivec2 numTiles = calculateTileGridDims(tileSize, screenSize);
        uint32_t tileCount = numTiles.x * numTiles.y;

        std::vector<TileFrustum> tilePlanes(tileCount);
        for (int j = 0; j < numTiles.y; ++j)
        {
            for (int i = 0; i < numTiles.x; ++i)
            {
                Tile tile;
                for (int k = 0; k < 4; ++k)
                {
                    float x = tileSize.x * (i + k % 2);
                    float y = tileSize.y * (j + k / 2);
                    tile.screenSpacePoints[k] = glm::vec3(x, y, 1.0f);

                    glm::vec4 ndc = glm::vec4(tile.screenSpacePoints[k], 1.0f);
                    ndc.x /= screenSize.x; // in [0, 1]
                    ndc.y /= screenSize.y;
                    ndc.x = ndc.x * 2.0f - 1.0f; // in [-1, 1]
                    ndc.y = ndc.y * 2.0f - 1.0f;

                    glm::vec4 view = glm::inverse(projectionMatrix) * ndc;
                    tile.viewSpacePoints[k] = view / view.w;
                }

                tilePlanes[j * numTiles.x + i].frustumPlanes[0] = computePlaneFromSpan(tile.viewSpacePoints[1], tile.viewSpacePoints[0]);
                tilePlanes[j * numTiles.x + i].frustumPlanes[1] = computePlaneFromSpan(tile.viewSpacePoints[3], tile.viewSpacePoints[1]);
                tilePlanes[j * numTiles.x + i].frustumPlanes[2] = computePlaneFromSpan(tile.viewSpacePoints[2], tile.viewSpacePoints[3]);
                tilePlanes[j * numTiles.x + i].frustumPlanes[3] = computePlaneFromSpan(tile.viewSpacePoints[0], tile.viewSpacePoints[2]);
            }
        }

        return tilePlanes;
    }
}
