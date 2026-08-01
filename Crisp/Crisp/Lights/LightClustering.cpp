#include <Crisp/Lights/LightClustering.hpp>

#include <Crisp/Renderer/VulkanImageUtils.hpp>

#pragma warning(disable : 26451) // Arithmetic overflow.

namespace crisp {
namespace {
struct Tile {
    std::array<glm::vec3, 4> screenSpacePoints;
    std::array<glm::vec3, 4> viewSpacePoints;
};

glm::vec4 computePlaneFromSpan(const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 n = glm::normalize(glm::cross(a, b));
    return {n, glm::dot(n, a)};
}

} // namespace

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize) {
    return (glm::ivec2(screenSize) - glm::ivec2(1)) / tileSize + glm::ivec2(1);
}

std::vector<TileFrustum> createTileFrusta(
    glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix) {
    glm::ivec2 numTiles = calculateTileGridDims(tileSize, screenSize);
    uint32_t tileCount = numTiles.x * numTiles.y;

    std::vector<TileFrustum> tilePlanes(tileCount);
    for (int j = 0; j < numTiles.y; ++j) {
        for (int i = 0; i < numTiles.x; ++i) {
            Tile tile{};
            for (int k = 0; k < 4; ++k) {
                const float x = static_cast<float>(tileSize.x) * static_cast<float>(i + k % 2);
                const float y = static_cast<float>(tileSize.y) * static_cast<float>(j + k / 2); // NOLINT
                tile.screenSpacePoints[k] = glm::vec3(x, y, 1.0f);

                glm::vec4 ndc = glm::vec4(tile.screenSpacePoints[k], 1.0f);
                ndc.x /= static_cast<float>(screenSize.x); // in [0, 1]
                ndc.y /= static_cast<float>(screenSize.y);
                ndc.x = ndc.x * 2.0f - 1.0f; // in [-1, 1]
                ndc.y = ndc.y * 2.0f - 1.0f;

                glm::vec4 view = glm::inverse(projectionMatrix) * ndc;
                tile.viewSpacePoints[k] = view / view.w;
            }

            tilePlanes[j * numTiles.x + i].frustumPlanes[0] =
                computePlaneFromSpan(tile.viewSpacePoints[1], tile.viewSpacePoints[0]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[1] =
                computePlaneFromSpan(tile.viewSpacePoints[3], tile.viewSpacePoints[1]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[2] =
                computePlaneFromSpan(tile.viewSpacePoints[2], tile.viewSpacePoints[3]);
            tilePlanes[j * numTiles.x + i].frustumPlanes[3] =
                computePlaneFromSpan(tile.viewSpacePoints[0], tile.viewSpacePoints[2]);
        }
    }

    return tilePlanes;
}

void LightClustering::configure(
    Renderer* renderer, const CameraParameters& cameraParameters, const uint32_t maximumLightCount) {
    m_tileSize = glm::ivec2(16);
    m_gridSize = calculateTileGridDims(m_tileSize, cameraParameters.screenSize);
    const auto tileFrusta{createTileFrusta(m_tileSize, cameraParameters.screenSize, cameraParameters.P)};
    const std::size_t tileCount = tileFrusta.size();

    m_tilePlaneBuffer =
        createStorageRingBuffer(&renderer->getDevice(), tileCount * sizeof(TileFrustum), tileFrusta.data());
    m_lightIndexCountBuffer = createStorageRingBuffer(&renderer->getDevice(), sizeof(uint32_t));
    m_lightIndexListBuffer =
        createStorageRingBuffer(&renderer->getDevice(), tileCount * sizeof(uint32_t) * maximumLightCount);

    m_lightGrid = createSampledStorageImage(
        *renderer,
        VK_FORMAT_R32G32_UINT,
        VkExtent3D{static_cast<uint32_t>(m_gridSize.x), static_cast<uint32_t>(m_gridSize.y), 1u});
    m_lightGridView = createView(renderer->getDevice(), *m_lightGrid, VK_IMAGE_VIEW_TYPE_2D);

    renderer->enqueueResourceUpdate([this](VkCommandBuffer cmdBuffer) {
        m_lightGrid->transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, kNullStage >> kComputeRead);
    });
}

} // namespace crisp
