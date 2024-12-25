#pragma once

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Renderer/UniformBuffer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImage.hpp>
#include <Crisp/Vulkan/Rhi/VulkanImageView.hpp>

#include <vector>

namespace crisp {
class Renderer;
class RenderGraph;

struct TileFrustum {
    glm::vec4 frustumPlanes[4];
};

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize);
std::vector<TileFrustum> createTileFrusta(glm::ivec2 tileSize, glm::ivec2 screenSize, const glm::mat4& projectionMatrix);

struct LightClustering {
    // Size of a single tile in pixel coordinates, e.g. 16x16.
    glm::ivec2 m_tileSize;

    // Size of the grid, e.g. number of tiles in X and Y needed to cover the whole screen.
    glm::ivec2 m_gridSize;

    // Buffer that holds all tile planes. We are not accounting for depth, so 4x number of tiles only.
    std::unique_ptr<UniformBuffer> m_tilePlaneBuffer;

    //
    std::unique_ptr<UniformBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<UniformBuffer> m_lightIndexListBuffer;
    std::unique_ptr<VulkanImage> m_lightGrid;
    std::vector<std::unique_ptr<VulkanImageView>> m_lightGridViews;

    void configure(Renderer* renderer, const CameraParameters& cameraParameters, uint32_t maximumLightCount);
};

void addToRenderGraph(
    Renderer* renderer,
    RenderGraph& renderGraph,
    const LightClustering& lightClustering,
    const UniformBuffer& cameraBuffer,
    const UniformBuffer& pointLightBuffer);

} // namespace crisp