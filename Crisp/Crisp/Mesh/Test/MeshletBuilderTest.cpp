#include <gmock/gmock.h>

#include <set>

#include <Crisp/Mesh/MeshletBuilder.hpp>
#include <Crisp/Mesh/TriangleMeshUtils.hpp>

namespace crisp {
namespace {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Le;
using ::testing::Not;

std::multiset<std::array<uint32_t, 3>> collectMeshletTriangles(const MeshletGeometry& geometry) {
    std::multiset<std::array<uint32_t, 3>> triangles;
    for (const auto& meshlet : geometry.meshlets) {
        for (uint32_t i = 0; i < meshlet.triangleCount; ++i) {
            const uint32_t offset = meshlet.triangleOffset + i * 3;
            triangles.insert({
                geometry.vertices[meshlet.vertexOffset + geometry.triangles[offset]],
                geometry.vertices[meshlet.vertexOffset + geometry.triangles[offset + 1]],
                geometry.vertices[meshlet.vertexOffset + geometry.triangles[offset + 2]],
            });
        }
    }
    return triangles;
}

std::multiset<std::array<uint32_t, 3>> collectMeshTriangles(const TriangleMesh& mesh) {
    std::multiset<std::array<uint32_t, 3>> triangles;
    for (const auto& triangle : mesh.getTriangles()) {
        triangles.insert({triangle.x, triangle.y, triangle.z});
    }
    return triangles;
}

TEST(MeshletBuilderTest, CoversEveryTriangleExactlyOnce) {
    const TriangleMesh mesh{createSphereMesh()};

    const auto geometry = buildMeshlets(mesh);

    EXPECT_THAT(geometry.meshlets, Not(IsEmpty()));
    EXPECT_THAT(collectMeshletTriangles(geometry), Eq(collectMeshTriangles(mesh)));
}

TEST(MeshletBuilderTest, RespectsTheClusterSizeLimits) {
    const TriangleMesh mesh{createSphereMesh()};
    const MeshletBuildOptions options{.maxVertices = 64, .maxTriangles = 124, .coneWeight = 0.25f};

    const auto geometry = buildMeshlets(mesh, options);

    for (const auto& meshlet : geometry.meshlets) {
        EXPECT_THAT(meshlet.vertexCount, Le(options.maxVertices));
        EXPECT_THAT(meshlet.triangleCount, Le(options.maxTriangles));
        EXPECT_THAT(meshlet.vertexCount, Gt(0u));
        EXPECT_THAT(meshlet.triangleCount, Gt(0u));
    }
    EXPECT_THAT(geometry.bounds.size(), Eq(geometry.meshlets.size()));
}

TEST(MeshletBuilderTest, BoundingSpheresContainTheirClusters) {
    const TriangleMesh mesh{createSphereMesh()};

    const auto geometry = buildMeshlets(mesh);

    ASSERT_THAT(geometry.meshlets, Not(IsEmpty()));
    for (size_t i = 0; i < geometry.meshlets.size(); ++i) {
        const auto& meshlet = geometry.meshlets[i];
        const glm::vec3 center{geometry.bounds[i].centerRadius};
        const float radius = geometry.bounds[i].centerRadius.w;

        for (uint32_t v = 0; v < meshlet.vertexCount; ++v) {
            const glm::vec3 position{mesh.getPositions()[geometry.vertices[meshlet.vertexOffset + v]]};
            EXPECT_THAT(glm::distance(position, center), Le(radius + 1e-4f)) << "meshlet " << i << ", vertex " << v;
        }
    }
}

TEST(MeshletBuilderTest, ConeCullingRejectsTheFarSideOfASphere) {
    const TriangleMesh mesh{createSphereMesh()};

    const auto geometry = buildMeshlets(mesh, {.coneWeight = 0.5f});

    ASSERT_THAT(geometry.meshlets.size(), Ge(8u));
    const glm::vec3 cameraPosition{0.0f, 0.0f, 10.0f};

    uint32_t culled{0};
    for (const auto& bounds : geometry.bounds) {
        if (!isMeshletConeVisible(bounds, cameraPosition)) {
            ++culled;

            EXPECT_THAT(bounds.centerRadius.z, Le(0.0f));
        }
    }

    EXPECT_THAT(culled, Gt(0u)) << "cone culling rejected nothing on a closed sphere";
    EXPECT_THAT(culled, Le(static_cast<uint32_t>(geometry.meshlets.size()))) << "cone culling rejected everything";
}

TEST(MeshletBuilderTest, TreatsAnUnboundedConeAsVisible) {
    MeshletBounds bounds{};
    bounds.coneApex = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    bounds.coneAxisCutoff = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

    EXPECT_TRUE(isMeshletConeVisible(bounds, glm::vec3(0.0f, 0.0f, 10.0f)));
    EXPECT_TRUE(isMeshletConeVisible(bounds, glm::vec3(0.0f, 0.0f, -10.0f)));
}

TEST(MeshletBuilderTest, LeavesAnEmptyMeshAlone) {
    const TriangleMesh mesh{};

    const auto geometry = buildMeshlets(mesh);

    EXPECT_THAT(geometry.meshlets, IsEmpty());
    EXPECT_THAT(geometry.bounds, IsEmpty());
}

} // namespace
} // namespace crisp
