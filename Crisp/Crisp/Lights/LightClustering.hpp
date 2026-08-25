#pragma once

#include <limits>
#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class Renderer;

inline constexpr uint32_t kMaxLightsPerTile = 1024;

struct TileFrustum {
    std::array<glm::vec4, 4> frustumPlanes;
};

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize);
std::vector<TileFrustum> createTileFrusta(glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix);

bool isSphereInsideTileFrustum(const glm::vec3& eyeCenter, float radius, const TileFrustum& frustum);
float viewDepthFromReverseZ(float depth, float zNear);
bool isSphereInsideTileDepthRange(const glm::vec3& eyeCenter, float radius, float tileNearZ, float tileFarZ);

struct LightClustering {
    // Size of a single tile in pixel coordinates
    glm::ivec2 m_tileSize;

    // Size of the grid, e.g. number of tiles in X and Y needed to cover the whole screen.
    glm::ivec2 m_gridSize;

    // Buffer that holds all tile planes. We are not accounting for depth, so 4x number of tiles only.
    std::unique_ptr<VulkanRingBuffer> m_tilePlaneBuffer;

    std::unique_ptr<VulkanRingBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<VulkanRingBuffer> m_lightIndexListBuffer;
    std::unique_ptr<VulkanImage> m_lightGrid;
    std::unique_ptr<VulkanImageView> m_lightGridView;

    void configure(Renderer* renderer, const CameraParameters& cameraParameters);
};

} // namespace crisp
