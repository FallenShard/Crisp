#include <Crisp/Lights/LightClustering.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include <Crisp/Renderer/Renderer.hpp>

namespace crisp {
namespace {

glm::vec3 unprojectPixel(const glm::vec2& pixel, const glm::vec2& screenSize, const glm::mat4& invProjection) {
    glm::vec4 ndc{pixel.x / screenSize.x * 2.0f - 1.0f, pixel.y / screenSize.y * 2.0f - 1.0f, 1.0f, 1.0f};
    const glm::vec4 view{invProjection * ndc};
    return glm::vec3(view) / view.w;
}

glm::vec3 rayAtViewDepth(const glm::vec3& ray, const float viewDepth) {
    return ray * (viewDepth / -ray.z);
}

} // namespace

glm::ivec2 calculateTileGridDims(const glm::ivec2 tileSize, const glm::ivec2 screenSize) {
    return (screenSize - glm::ivec2(1)) / tileSize + glm::ivec2(1);
}

glm::ivec3 calculateClusterGridDims(const int32_t tileSize, const int32_t sliceCount, const glm::ivec2 screenSize) {
    const glm::ivec2 tileDims{calculateTileGridDims(glm::ivec2(tileSize), screenSize)};
    return {tileDims.x, tileDims.y, sliceCount};
}

float clusterSliceViewDepth(const int32_t slice, const int32_t sliceCount, const float zNear, const float zFar) {
    const float t = static_cast<float>(slice) / static_cast<float>(sliceCount);
    return zNear * std::pow(zFar / zNear, t);
}

int32_t clusterSliceFromViewDepth(const float viewDepth, const int32_t sliceCount, const float zNear, const float zFar) {
    if (viewDepth <= zNear) {
        return 0;
    }
    const float slice = std::log(viewDepth / zNear) / std::log(zFar / zNear) * static_cast<float>(sliceCount);
    return std::clamp(static_cast<int32_t>(slice), 0, sliceCount - 1);
}

std::vector<ClusterAabb> createClusterAabbs(
    const int32_t tileSize,
    const int32_t sliceCount,
    const glm::ivec2 screenSize,
    const glm::mat4& projectionMatrix,
    const float zNear,
    const float zFar) {
    const glm::ivec3 gridDims{calculateClusterGridDims(tileSize, sliceCount, screenSize)};
    const glm::mat4 invProjection{glm::inverse(projectionMatrix)};
    const glm::vec2 screenSizeF{screenSize};

    std::vector<ClusterAabb> clusters(static_cast<size_t>(gridDims.x) * gridDims.y * gridDims.z);
    for (int32_t j = 0; j < gridDims.y; ++j) {
        for (int32_t i = 0; i < gridDims.x; ++i) {
            const glm::vec2 minPixel{static_cast<float>(i * tileSize), static_cast<float>(j * tileSize)};
            const glm::vec2 maxPixel{
                std::min(static_cast<float>((i + 1) * tileSize), screenSizeF.x),
                std::min(static_cast<float>((j + 1) * tileSize), screenSizeF.y)};

            const std::array<glm::vec3, 4> cornerRays{
                unprojectPixel({minPixel.x, minPixel.y}, screenSizeF, invProjection),
                unprojectPixel({maxPixel.x, minPixel.y}, screenSizeF, invProjection),
                unprojectPixel({minPixel.x, maxPixel.y}, screenSizeF, invProjection),
                unprojectPixel({maxPixel.x, maxPixel.y}, screenSizeF, invProjection),
            };

            for (int32_t k = 0; k < gridDims.z; ++k) {
                const float sliceNear{clusterSliceViewDepth(k, sliceCount, zNear, zFar)};
                const float sliceFar{clusterSliceViewDepth(k + 1, sliceCount, zNear, zFar)};

                glm::vec3 minPoint{std::numeric_limits<float>::max()};
                glm::vec3 maxPoint{std::numeric_limits<float>::lowest()};
                for (const auto& ray : cornerRays) {
                    for (const float depth : {sliceNear, sliceFar}) {
                        const glm::vec3 point{rayAtViewDepth(ray, depth)};
                        minPoint = glm::min(minPoint, point);
                        maxPoint = glm::max(maxPoint, point);
                    }
                }

                const size_t index{(static_cast<size_t>(k) * gridDims.y + j) * gridDims.x + i};
                clusters[index] = {glm::vec4(minPoint, 0.0f), glm::vec4(maxPoint, 0.0f)};
            }
        }
    }

    return clusters;
}

bool isSphereInsideClusterAabb(const glm::vec3& eyeCenter, const float radius, const ClusterAabb& aabb) {
    const glm::vec3 closest{glm::clamp(eyeCenter, glm::vec3(aabb.minPoint), glm::vec3(aabb.maxPoint))};
    const glm::vec3 delta{eyeCenter - closest};
    return glm::dot(delta, delta) <= radius * radius;
}

void LightClustering::configure(Renderer* renderer, const CameraParameters& cameraParameters) {
    m_clusterGridSize = calculateClusterGridDims(kClusterTileSize, kClusterDepthSliceCount, cameraParameters.screenSize);
    const auto clusters{createClusterAabbs(
        kClusterTileSize,
        kClusterDepthSliceCount,
        cameraParameters.screenSize,
        cameraParameters.P,
        cameraParameters.nearFar.x,
        cameraParameters.nearFar.y)};
    const uint32_t clusterCount{getClusterCount()};

    m_clusterAabbBuffer =
        createStorageRingBuffer(&renderer->getDevice(), clusters.size() * sizeof(ClusterAabb), clusters.data());
    m_lightIndexCountBuffer = createStorageRingBuffer(&renderer->getDevice(), sizeof(uint32_t));
    m_lightIndexListBuffer =
        createStorageRingBuffer(&renderer->getDevice(), clusterCount * sizeof(uint32_t) * kMaxLightsPerCluster);
    m_lightGridBuffer = createStorageRingBuffer(&renderer->getDevice(), clusterCount * sizeof(glm::uvec2));
}

} // namespace crisp
