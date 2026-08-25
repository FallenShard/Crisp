#include <gtest/gtest.h>

#include <cmath>

#include <Crisp/Camera/Camera.hpp>
#include <Crisp/Lights/LightClustering.hpp>

namespace crisp {
namespace {

constexpr glm::ivec2 kScreenSize{256, 128};
constexpr int32_t kTileSize{64};
constexpr int32_t kSliceCount{16};
constexpr float kZNear{0.1f};
constexpr float kZFar{1000.0f};

glm::mat4 createProjection() {
    return Camera(kScreenSize.x, kScreenSize.y).getProjectionMatrix();
}

std::vector<ClusterAabb> createTestClusters() {
    return createClusterAabbs(kTileSize, kSliceCount, kScreenSize, createProjection(), kZNear, kZFar);
}

size_t clusterIndex(const glm::ivec3 gridDims, const int32_t i, const int32_t j, const int32_t k) {
    return (static_cast<size_t>(k) * gridDims.y + j) * gridDims.x + i;
}

TEST(LightClusteringTest, TileGridCoversPartialTiles) {
    EXPECT_EQ(calculateTileGridDims({16, 16}, {128, 64}), glm::ivec2(8, 4));
    // A viewport that does not divide evenly still needs a tile for the remainder.
    EXPECT_EQ(calculateTileGridDims({16, 16}, {129, 65}), glm::ivec2(9, 5));
    EXPECT_EQ(calculateTileGridDims({16, 16}, {1920, 1080}), glm::ivec2(120, 68));
}

TEST(LightClusteringTest, ClusterGridIsTilesTimesSlices) {
    EXPECT_EQ(calculateClusterGridDims(64, 24, {1920, 1080}), glm::ivec3(30, 17, 24));
    EXPECT_EQ(calculateClusterGridDims(kTileSize, kSliceCount, kScreenSize), glm::ivec3(4, 2, kSliceCount));
}

TEST(LightClusteringTest, ProducesOneAabbPerCluster) {
    const auto clusters = createTestClusters();
    EXPECT_EQ(clusters.size(), 4u * 2u * static_cast<size_t>(kSliceCount));
}

TEST(LightClusteringTest, SlicesAreDistributedExponentially) {
    EXPECT_FLOAT_EQ(clusterSliceViewDepth(0, kSliceCount, kZNear, kZFar), kZNear);
    EXPECT_FLOAT_EQ(clusterSliceViewDepth(kSliceCount, kSliceCount, kZNear, kZFar), kZFar);

    const float firstRatio =
        clusterSliceViewDepth(1, kSliceCount, kZNear, kZFar) / clusterSliceViewDepth(0, kSliceCount, kZNear, kZFar);
    for (int32_t k = 1; k < kSliceCount; ++k) {
        const float ratio =
            clusterSliceViewDepth(k + 1, kSliceCount, kZNear, kZFar) /
            clusterSliceViewDepth(k, kSliceCount, kZNear, kZFar);
        EXPECT_NEAR(ratio, firstRatio, 1e-4f);
    }

    const float nearSpan = clusterSliceViewDepth(1, kSliceCount, kZNear, kZFar) - kZNear;
    const float farSpan = kZFar - clusterSliceViewDepth(kSliceCount - 1, kSliceCount, kZNear, kZFar);
    EXPECT_GT(farSpan, nearSpan);
}

TEST(LightClusteringTest, SliceLookupInvertsSliceBounds) {
    for (int32_t k = 0; k < kSliceCount; ++k) {
        const float sliceNear = clusterSliceViewDepth(k, kSliceCount, kZNear, kZFar);
        const float sliceFar = clusterSliceViewDepth(k + 1, kSliceCount, kZNear, kZFar);
        const float middle = std::sqrt(sliceNear * sliceFar);
        EXPECT_EQ(clusterSliceFromViewDepth(middle, kSliceCount, kZNear, kZFar), k);
    }
}

TEST(LightClusteringTest, SliceLookupClampsOutsideTheRange) {
    EXPECT_EQ(clusterSliceFromViewDepth(0.0f, kSliceCount, kZNear, kZFar), 0);
    EXPECT_EQ(clusterSliceFromViewDepth(kZNear * 0.5f, kSliceCount, kZNear, kZFar), 0);
    EXPECT_EQ(clusterSliceFromViewDepth(kZFar * 10.0f, kSliceCount, kZNear, kZFar), kSliceCount - 1);
}

TEST(LightClusteringTest, ClusterBoundsSpanTheirSlice) {
    const auto clusters = createTestClusters();
    const glm::ivec3 gridDims{calculateClusterGridDims(kTileSize, kSliceCount, kScreenSize)};

    for (int32_t k = 0; k < gridDims.z; ++k) {
        const float sliceNear = clusterSliceViewDepth(k, kSliceCount, kZNear, kZFar);
        const float sliceFar = clusterSliceViewDepth(k + 1, kSliceCount, kZNear, kZFar);
        for (int32_t j = 0; j < gridDims.y; ++j) {
            for (int32_t i = 0; i < gridDims.x; ++i) {
                const auto& aabb = clusters[clusterIndex(gridDims, i, j, k)];
                EXPECT_NEAR(aabb.maxPoint.z, -sliceNear, 1e-3f);
                EXPECT_NEAR(aabb.minPoint.z, -sliceFar, 1e-3f);
                EXPECT_LE(aabb.minPoint.x, aabb.maxPoint.x);
                EXPECT_LE(aabb.minPoint.y, aabb.maxPoint.y);
            }
        }
    }
}

TEST(LightClusteringTest, AcceptsALightSittingInsideTheCluster) {
    const auto clusters = createTestClusters();
    const glm::ivec3 gridDims{calculateClusterGridDims(kTileSize, kSliceCount, kScreenSize)};

    for (size_t index = 0; index < clusters.size(); ++index) {
        const auto& aabb = clusters[index];
        const glm::vec3 center{(glm::vec3(aabb.minPoint) + glm::vec3(aabb.maxPoint)) * 0.5f};
        EXPECT_TRUE(isSphereInsideClusterAabb(center, 0.001f, aabb)) << "cluster " << index;
    }
    EXPECT_EQ(clusters.size(), static_cast<size_t>(gridDims.x * gridDims.y * gridDims.z));
}

TEST(LightClusteringTest, RejectsALightInTheSameTileButADifferentSlice) {
    const auto clusters = createTestClusters();
    const glm::ivec3 gridDims{calculateClusterGridDims(kTileSize, kSliceCount, kScreenSize)};

    const auto& nearCluster = clusters[clusterIndex(gridDims, 2, 1, 2)];
    const auto& farCluster = clusters[clusterIndex(gridDims, 2, 1, gridDims.z - 1)];

    const glm::vec3 farCenter{(glm::vec3(farCluster.minPoint) + glm::vec3(farCluster.maxPoint)) * 0.5f};
    EXPECT_TRUE(isSphereInsideClusterAabb(farCenter, 0.001f, farCluster));
    EXPECT_FALSE(isSphereInsideClusterAabb(farCenter, 0.001f, nearCluster));
}

TEST(LightClusteringTest, RejectsALightBeyondItsRadius) {
    const auto clusters = createTestClusters();
    const glm::ivec3 gridDims{calculateClusterGridDims(kTileSize, kSliceCount, kScreenSize)};
    const auto& cluster = clusters[clusterIndex(gridDims, 0, 0, 4)];

    const glm::vec3 center{(glm::vec3(cluster.minPoint) + glm::vec3(cluster.maxPoint)) * 0.5f};
    const glm::vec3 extents{glm::vec3(cluster.maxPoint) - glm::vec3(cluster.minPoint)};
    const glm::vec3 outside{center + glm::vec3(extents.x + 10.0f, 0.0f, 0.0f)};

    EXPECT_FALSE(isSphereInsideClusterAabb(outside, 1.0f, cluster));
    EXPECT_TRUE(isSphereInsideClusterAabb(outside, extents.x + 20.0f, cluster));
}

} // namespace
} // namespace crisp
