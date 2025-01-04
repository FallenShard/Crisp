#pragma once

#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class Renderer;
class RenderGraph;

struct TileFrustum {
    std::array<glm::vec4, 4> frustumPlanes;
};

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize);
std::vector<TileFrustum> createTileFrusta(glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix);

struct LightClustering {
    // Size of a single tile in pixel coordinates, e.g. 16x16.
    glm::ivec2 m_tileSize;

    // Size of the grid, e.g. number of tiles in X and Y needed to cover the whole screen.
    glm::ivec2 m_gridSize;

    // Buffer that holds all tile planes. We are not accounting for depth, so 4x number of tiles only.
    std::unique_ptr<VulkanRingBuffer> m_tilePlaneBuffer;

    std::unique_ptr<VulkanRingBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<VulkanRingBuffer> m_lightIndexListBuffer;
    std::unique_ptr<VulkanImage> m_lightGrid;
    std::vector<std::unique_ptr<VulkanImageView>> m_lightGridViews;

    void configure(Renderer* renderer, const CameraParameters& cameraParameters, uint32_t maximumLightCount);
};

void addToRenderGraph(
    Renderer* renderer,
    RenderGraph& renderGraph,
    const LightClustering& lightClustering,
    const VulkanRingBuffer& cameraBuffer,
    const VulkanRingBuffer& pointLightBuffer);

} // namespace crisp