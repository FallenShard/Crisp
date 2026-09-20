#include <gmock/gmock.h>

#include <algorithm>
#include <cstring>
#include <random>

#include <Crisp/Mesh/MeshOptimizer.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>

namespace crisp {
namespace {

using ::testing::Eq;
using ::testing::Le;
using ::testing::Lt;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

struct ResolvedTriangle {
    std::array<glm::vec3, 3> positions;
    std::array<glm::vec3, 3> normals;

    bool operator==(const ResolvedTriangle& other) const = default;
};

std::vector<ResolvedTriangle> resolveTriangles(
    const TriangleMesh& mesh, const uint32_t firstTriangle = 0, const uint32_t triangleCount = ~0u) {
    const auto& triangles = mesh.getTriangles();
    const uint32_t last = triangleCount == ~0u ? static_cast<uint32_t>(triangles.size()) : firstTriangle + triangleCount;

    std::vector<ResolvedTriangle> resolved;
    resolved.reserve(last - firstTriangle);
    for (uint32_t i = firstTriangle; i < last; ++i) {
        const auto& triangle = triangles[i];
        ResolvedTriangle entry{};
        for (uint32_t corner = 0; corner < 3; ++corner) {
            const uint32_t index = triangle[corner];
            entry.positions[corner] = mesh.getPositions()[index];
            entry.normals[corner] = mesh.hasNormals() ? mesh.getNormals()[index] : glm::vec3(0.0f);
        }
        resolved.push_back(entry);
    }
    return resolved;
}

TriangleMesh createShuffledGrid(const int32_t tessellation) {
    TriangleMesh mesh{createGridMesh(4.0f, tessellation)};

    auto triangles = mesh.getTriangles();
    std::mt19937 rng{1337};
    std::shuffle(triangles.begin(), triangles.end(), rng);
    mesh.setTriangles(std::move(triangles));
    return mesh;
}

TEST(MeshOptimizerTest, PreservesTheSurface) {
    TriangleMesh mesh{createShuffledGrid(16)};
    const auto before = resolveTriangles(mesh);

    optimizeMeshIndices(mesh);

    EXPECT_THAT(mesh.getTriangles(), SizeIs(before.size()));
    EXPECT_THAT(resolveTriangles(mesh), UnorderedElementsAreArray(before));
}

TEST(MeshOptimizerTest, ImprovesVertexCacheAndFetchLocality) {
    TriangleMesh mesh{createShuffledGrid(32)};

    const auto stats = optimizeMeshIndices(mesh);

    EXPECT_THAT(stats.atvrAfter, Lt(stats.atvrBefore));
    EXPECT_THAT(stats.acmrAfter, Lt(stats.acmrBefore));
    EXPECT_THAT(stats.overfetchAfter, Le(stats.overfetchBefore));
    EXPECT_THAT(stats.atvrAfter, Lt(1.3f));
}

TEST(MeshOptimizerTest, WeldsVerticesThatEveryStreamAgreesAreIdentical) {
    TriangleMesh mesh{
        {{0.0f, 0.0f, 0.0f},
         {1.0f, 0.0f, 0.0f},
         {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f},
         {1.0f, 1.0f, 0.0f},
         {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}},
        {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}},
        {{0, 1, 2}, {3, 4, 5}}};
    const auto before = resolveTriangles(mesh);

    const auto stats = optimizeMeshIndices(mesh);

    EXPECT_THAT(stats.vertexCountBefore, Eq(6u));
    EXPECT_THAT(mesh.getVertexCount(), Eq(4u));
    EXPECT_THAT(stats.vertexCountAfter, Eq(4u));
    EXPECT_THAT(resolveTriangles(mesh), UnorderedElementsAreArray(before));
}

TEST(MeshOptimizerTest, KeepsVerticesThatDifferInOneStreamApart) {
    TriangleMesh mesh{
        {{0.0f, 0.0f, 0.0f},
         {1.0f, 0.0f, 0.0f},
         {0.0f, 1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f},
         {1.0f, 1.0f, 0.0f},
         {0.0f, 1.0f, 0.0f}},
        {},
        {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 0.5f}, {1.0f, 1.0f}, {0.25f, 0.25f}},
        {{0, 1, 2}, {3, 4, 5}}};

    optimizeMeshIndices(mesh);

    EXPECT_THAT(mesh.getVertexCount(), Eq(6u));
}

TEST(MeshOptimizerTest, CarriesCustomAttributesWithTheirVertices) {
    TriangleMesh mesh{createShuffledGrid(8)};

    const auto tagOf = [](const glm::vec3& position) {
        return static_cast<uint32_t>((position.x + 8.0f) * 1000.0f) * 100003u +
               static_cast<uint32_t>((position.z + 8.0f) * 1000.0f);
    };
    std::vector<uint32_t> tags;
    tags.reserve(mesh.getVertexCount());
    for (const auto& position : mesh.getPositions()) {
        tags.push_back(tagOf(position));
    }
    mesh.setCustomAttribute("tag", createCustomVertexAttributeBuffer<uint32_t>(tags));

    optimizeMeshIndices(mesh);

    const auto& attribute = mesh.getCustomAttribute("tag");
    ASSERT_THAT(attribute.buffer.size(), Eq(mesh.getVertexCount() * sizeof(uint32_t)));
    std::vector<uint32_t> remappedTags(mesh.getVertexCount());
    std::memcpy(remappedTags.data(), attribute.buffer.data(), attribute.buffer.size());

    for (uint32_t i = 0; i < mesh.getVertexCount(); ++i) {
        EXPECT_THAT(remappedTags[i], Eq(tagOf(mesh.getPositions()[i]))) << "vertex " << i;
    }
}

TEST(MeshOptimizerTest, KeepsEachViewsTrianglesInItsOwnRange) {
    TriangleMesh mesh{createShuffledGrid(8)};
    const uint32_t triangleCount = mesh.getTriangleCount();
    const uint32_t firstViewTriangles = triangleCount / 3;

    std::vector<TriangleMeshView> views;
    views.emplace_back("first", 0, firstViewTriangles * 3);
    views.emplace_back("second", firstViewTriangles * 3, (triangleCount - firstViewTriangles) * 3);
    mesh.setViews(std::move(views));

    const auto firstBefore = resolveTriangles(mesh, 0, firstViewTriangles);
    const auto secondBefore = resolveTriangles(mesh, firstViewTriangles, triangleCount - firstViewTriangles);

    optimizeMeshIndices(mesh);

    ASSERT_THAT(mesh.getViews(), SizeIs(2));
    EXPECT_THAT(mesh.getViews()[0].firstIndex, Eq(0u));
    EXPECT_THAT(mesh.getViews()[0].indexCount, Eq(firstViewTriangles * 3));

    EXPECT_THAT(resolveTriangles(mesh, 0, firstViewTriangles), UnorderedElementsAreArray(firstBefore));
    EXPECT_THAT(
        resolveTriangles(mesh, firstViewTriangles, triangleCount - firstViewTriangles),
        UnorderedElementsAreArray(secondBefore));
}

TEST(MeshOptimizerTest, LeavesAnEmptyMeshAlone) {
    TriangleMesh mesh{};

    const auto stats = optimizeMeshIndices(mesh);

    EXPECT_THAT(stats.indexCount, Eq(0u));
    EXPECT_THAT(mesh.getTriangleCount(), Eq(0u));
}

} // namespace
} // namespace crisp
