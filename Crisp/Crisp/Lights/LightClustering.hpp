#pragma once

#include <vector>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Vulkan/VulkanRingBuffer.hpp>

namespace crisp {
class Renderer;

inline constexpr uint32_t kMaxLightsPerCluster = 256;
inline constexpr int32_t kClusterTileSize = 64;
inline constexpr int32_t kClusterDepthSliceCount = 24;

struct ClusterAabb {
    glm::vec4 minPoint;
    glm::vec4 maxPoint;
};

glm::ivec2 calculateTileGridDims(glm::ivec2 tileSize, glm::ivec2 screenSize);
glm::ivec3 calculateClusterGridDims(int32_t tileSize, int32_t sliceCount, glm::ivec2 screenSize);

float clusterSliceViewDepth(int32_t slice, int32_t sliceCount, float zNear, float zFar);
int32_t clusterSliceFromViewDepth(float viewDepth, int32_t sliceCount, float zNear, float zFar);

std::vector<ClusterAabb> createClusterAabbs(
    int32_t tileSize,
    int32_t sliceCount,
    glm::ivec2 screenSize,
    const glm::mat4& projectionMatrix,
    float zNear,
    float zFar);

bool isSphereInsideClusterAabb(const glm::vec3& eyeCenter, float radius, const ClusterAabb& aabb);

struct LightClustering {
    glm::ivec3 m_clusterGridSize;

    std::unique_ptr<VulkanRingBuffer> m_clusterAabbBuffer;
    std::unique_ptr<VulkanRingBuffer> m_lightIndexCountBuffer;
    std::unique_ptr<VulkanRingBuffer> m_lightIndexListBuffer;

    std::unique_ptr<VulkanRingBuffer> m_lightGridBuffer;

    uint32_t getClusterCount() const {
        return static_cast<uint32_t>(m_clusterGridSize.x * m_clusterGridSize.y * m_clusterGridSize.z);
    }

    void configure(Renderer* renderer, const CameraParameters& cameraParameters);
};

} // namespace crisp
