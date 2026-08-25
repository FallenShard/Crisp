#include <gtest/gtest.h>

#include <cmath>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/LightClustering.hpp>

namespace crisp {
namespace {

constexpr glm::ivec2 kScreenSize{128, 64};
constexpr glm::ivec2 kTileSize{16, 16};

glm::vec3 tileCenterRayPoint(const glm::mat4& projection, const glm::ivec2 tileCoord, const float depth) {
    const glm::vec2 pixel{
        (static_cast<float>(tileCoord.x) + 0.5f) * static_cast<float>(kTileSize.x),
        (static_cast<float>(tileCoord.y) + 0.5f) * static_cast<float>(kTileSize.y)};
    glm::vec4 ndc{
        pixel.x / static_cast<float>(kScreenSize.x) * 2.0f - 1.0f,
        pixel.y / static_cast<float>(kScreenSize.y) * 2.0f - 1.0f,
        1.0f,
        1.0f};
    const glm::vec4 view = glm::inverse(projection) * ndc;
    const glm::vec3 nearPoint{glm::vec3(view) / view.w};
    return nearPoint * (depth / -nearPoint.z);
}

glm::mat4 createProjection() {
    return Camera(kScreenSize.x, kScreenSize.y).getProjectionMatrix();
}

TEST(LightClusteringTest, TileGridCoversPartialTiles) {
    EXPECT_EQ(calculateTileGridDims(kTileSize, {128, 64}), glm::ivec2(8, 4));
    // A viewport that does not divide evenly still needs a tile for the remainder.
    EXPECT_EQ(calculateTileGridDims(kTileSize, {129, 65}), glm::ivec2(9, 5));
    EXPECT_EQ(calculateTileGridDims(kTileSize, {1920, 1080}), glm::ivec2(120, 68));
}

TEST(LightClusteringTest, ProducesOneFrustumPerTile) {
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, createProjection());
    EXPECT_EQ(frusta.size(), 8u * 4u);
}

TEST(LightClusteringTest, TilePlanesPassThroughTheViewOrigin) {
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, createProjection());
    for (const auto& frustum : frusta) {
        for (const auto& plane : frustum.frustumPlanes) {
            EXPECT_NEAR(plane.w, 0.0f, 1e-5f);
            EXPECT_NEAR(glm::length(glm::vec3(plane)), 1.0f, 1e-5f);
        }
    }
}

TEST(LightClusteringTest, TilePlaneNormalsPointOutOfTheFrustum) {
    const auto projection = createProjection();
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, projection);
    const glm::ivec2 gridDims{calculateTileGridDims(kTileSize, kScreenSize)};

    for (int32_t y = 0; y < gridDims.y; ++y) {
        for (int32_t x = 0; x < gridDims.x; ++x) {
            const auto& frustum = frusta[y * gridDims.x + x];
            const glm::vec3 interior{tileCenterRayPoint(projection, {x, y}, 10.0f)};
            for (const auto& plane : frustum.frustumPlanes) {
                EXPECT_LT(glm::dot(glm::vec3(plane), interior) - plane.w, 0.0f)
                    << "Tile (" << x << ", " << y << ") centre landed outside one of its own planes.";
            }
        }
    }
}

TEST(LightClusteringTest, AcceptsAPointLightOnTheTileAxis) {
    const auto projection = createProjection();
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, projection);
    const glm::ivec2 gridDims{calculateTileGridDims(kTileSize, kScreenSize)};

    for (int32_t y = 0; y < gridDims.y; ++y) {
        for (int32_t x = 0; x < gridDims.x; ++x) {
            const glm::vec3 center{tileCenterRayPoint(projection, {x, y}, 10.0f)};
            EXPECT_TRUE(isSphereInsideTileFrustum(center, 1.0f, frusta[y * gridDims.x + x]));
        }
    }
}

TEST(LightClusteringTest, AcceptsALightWhoseRadiusDwarfsTheTile) {
    const auto projection = createProjection();
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, projection);
    const glm::ivec2 gridDims{calculateTileGridDims(kTileSize, kScreenSize)};

    const glm::ivec2 tile{gridDims.x / 2, gridDims.y / 2};
    const glm::vec3 center{tileCenterRayPoint(projection, tile, 50.0f)};
    EXPECT_TRUE(isSphereInsideTileFrustum(center, 100.0f, frusta[tile.y * gridDims.x + tile.x]));
}

TEST(LightClusteringTest, RejectsALightBehindTheOppositeEdge) {
    const auto projection = createProjection();
    const auto frusta = createTileFrusta(kTileSize, kScreenSize, projection);
    const glm::ivec2 gridDims{calculateTileGridDims(kTileSize, kScreenSize)};

    const glm::vec3 farRight{tileCenterRayPoint(projection, {gridDims.x - 1, gridDims.y / 2}, 10.0f)};
    const auto& leftTile = frusta[(gridDims.y / 2) * gridDims.x];
    EXPECT_FALSE(isSphereInsideTileFrustum(farRight, 0.01f, leftTile));
    EXPECT_TRUE(isSphereInsideTileFrustum(farRight, 100.0f, leftTile));
}

TEST(LightClusteringTest, InvertsReverseZDepthBackToViewSpace) {
    const Camera camera(kScreenSize.x, kScreenSize.y);
    const glm::mat4 projection{camera.getProjectionMatrix()};
    const float zNear{camera.getViewDepthRange().x};

    for (const float viewZ : {-0.5f, -1.0f, -10.0f, -250.0f, -5000.0f}) {
        const glm::vec4 clip{projection * glm::vec4(0.0f, 0.0f, viewZ, 1.0f)};
        const float depth{clip.z / clip.w};
        EXPECT_NEAR(viewDepthFromReverseZ(depth, zNear), viewZ, std::abs(viewZ) * 1e-3f);
    }
}

TEST(LightClusteringTest, MapsTheNearPlaneToDepthOne) {
    const Camera camera(kScreenSize.x, kScreenSize.y);
    const float zNear{camera.getViewDepthRange().x};
    EXPECT_FLOAT_EQ(viewDepthFromReverseZ(1.0f, zNear), -zNear);
}

TEST(LightClusteringTest, TreatsClearedDepthAsInfinitelyFar) {
    EXPECT_TRUE(std::isinf(viewDepthFromReverseZ(0.0f, 0.1f)));
    EXPECT_LT(viewDepthFromReverseZ(0.0f, 0.1f), 0.0f);
}

TEST(LightClusteringTest, RejectsLightsOutsideTheTileDepthRange) {
    constexpr float kTileNearZ = -10.0f;
    constexpr float kTileFarZ = -20.0f;

    EXPECT_FALSE(isSphereInsideTileDepthRange({0.0f, 0.0f, -5.0f}, 1.0f, kTileNearZ, kTileFarZ));
    EXPECT_FALSE(isSphereInsideTileDepthRange({0.0f, 0.0f, -30.0f}, 1.0f, kTileNearZ, kTileFarZ));
    EXPECT_TRUE(isSphereInsideTileDepthRange({0.0f, 0.0f, -15.0f}, 1.0f, kTileNearZ, kTileFarZ));
    EXPECT_TRUE(isSphereInsideTileDepthRange({0.0f, 0.0f, -5.0f}, 5.0f, kTileNearZ, kTileFarZ));
    EXPECT_TRUE(isSphereInsideTileDepthRange({0.0f, 0.0f, -30.0f}, 10.0f, kTileNearZ, kTileFarZ));
}

TEST(LightClusteringTest, KeepsLightsWhenTheTileHasNoFarBound) {
    const float farZ{viewDepthFromReverseZ(0.0f, 0.1f)};
    EXPECT_TRUE(isSphereInsideTileDepthRange({0.0f, 0.0f, -1000.0f}, 1.0f, -10.0f, farZ));
}

} // namespace
} // namespace crisp
