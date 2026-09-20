#include <Crisp/Mesh/MeshletBuilder.hpp>

#include <meshoptimizer.h>

#include <Crisp/Core/Checks.hpp>

namespace crisp {
namespace {

constexpr uint32_t kMaxMeshletVertices = 256;
constexpr uint32_t kMaxMeshletTriangles = 512;

} // namespace

MeshletBounds MeshletBounds::transformedBy(const glm::mat4& modelMatrix) const {
    const float scaleX = glm::length(glm::vec3(modelMatrix[0]));
    const float scaleY = glm::length(glm::vec3(modelMatrix[1]));
    const float scaleZ = glm::length(glm::vec3(modelMatrix[2]));
    const float maxScale = std::max({scaleX, scaleY, scaleZ});
    const float minScale = std::min({scaleX, scaleY, scaleZ});

    MeshletBounds result{};
    result.centerRadius =
        glm::vec4(glm::vec3(modelMatrix * glm::vec4(glm::vec3(centerRadius), 1.0f)), centerRadius.w * maxScale);
    result.coneApex = glm::vec4(glm::vec3(modelMatrix * glm::vec4(glm::vec3(coneApex), 1.0f)), 0.0f);

    const glm::mat3 normalMatrix{glm::transpose(glm::inverse(glm::mat3(modelMatrix)))};
    const glm::vec3 axis{normalMatrix * glm::vec3(coneAxisCutoff)};
    const float axisLength = glm::length(axis);

    const bool uniformScale = maxScale <= minScale * 1.001f;
    const float cutoff = uniformScale && axisLength > 0.0f ? coneAxisCutoff.w : 1.0f;
    result.coneAxisCutoff = glm::vec4(axisLength > 0.0f ? axis / axisLength : glm::vec3(0.0f, 0.0f, 1.0f), cutoff);
    return result;
}

MeshletGeometry buildMeshlets(const TriangleMesh& mesh, const MeshletBuildOptions& options) {
    CRISP_CHECK_GE_LT(options.maxVertices, 1u, kMaxMeshletVertices + 1);
    CRISP_CHECK_GE_LT(options.maxTriangles, 1u, kMaxMeshletTriangles + 1);
    CRISP_CHECK_GE_LT(options.coneWeight, 0.0f, 1.0f + std::numeric_limits<float>::epsilon());

    MeshletGeometry geometry{};
    geometry.maxVertices = options.maxVertices;
    geometry.maxTriangles = options.maxTriangles;
    if (!mesh.hasTriangles() || !mesh.hasPositions()) {
        return geometry;
    }

    const size_t indexCount = mesh.getIndexCount();
    const size_t vertexCount = mesh.getVertexCount();
    const size_t maxMeshletCount = meshopt_buildMeshletsBound(indexCount, options.maxVertices, options.maxTriangles);

    std::vector<meshopt_Meshlet> builtMeshlets(maxMeshletCount);
    geometry.vertices.resize(maxMeshletCount * options.maxVertices);
    geometry.triangles.resize(maxMeshletCount * options.maxTriangles * 3);

    const size_t meshletCount = meshopt_buildMeshlets(
        builtMeshlets.data(),
        geometry.vertices.data(),
        geometry.triangles.data(),
        mesh.getIndices(),
        indexCount,
        mesh.getPositionsPtr(),
        vertexCount,
        sizeof(glm::vec3),
        options.maxVertices,
        options.maxTriangles,
        options.coneWeight);
    if (meshletCount == 0) {
        geometry.vertices.clear();
        geometry.triangles.clear();
        return geometry;
    }

    const meshopt_Meshlet& last = builtMeshlets[meshletCount - 1];
    geometry.vertices.resize(last.vertex_offset + last.vertex_count);
    geometry.triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3u));
    geometry.meshlets.reserve(meshletCount);
    geometry.bounds.reserve(meshletCount);
    for (const auto& built : std::span{builtMeshlets}.first(meshletCount)) {
        // Reordering within a cluster costs nothing at runtime and improves locality for whoever consumes it.
        meshopt_optimizeMeshlet(
            &geometry.vertices[built.vertex_offset],
            &geometry.triangles[built.triangle_offset],
            built.triangle_count,
            built.vertex_count);

        geometry.meshlets.push_back(
            Meshlet{
                .vertexOffset = built.vertex_offset,
                .triangleOffset = built.triangle_offset,
                .vertexCount = built.vertex_count,
                .triangleCount = built.triangle_count,
            });

        const meshopt_Bounds meshletBounds = meshopt_computeMeshletBounds(
            &geometry.vertices[built.vertex_offset],
            &geometry.triangles[built.triangle_offset],
            built.triangle_count,
            mesh.getPositionsPtr(),
            vertexCount,
            sizeof(glm::vec3));

        geometry.bounds.push_back(
            MeshletBounds{
                .centerRadius = glm::vec4(
                    meshletBounds.center[0], meshletBounds.center[1], meshletBounds.center[2], meshletBounds.radius),
                .coneApex =
                    glm::vec4(meshletBounds.cone_apex[0], meshletBounds.cone_apex[1], meshletBounds.cone_apex[2], 0.0f),
                .coneAxisCutoff = glm::vec4(
                    meshletBounds.cone_axis[0],
                    meshletBounds.cone_axis[1],
                    meshletBounds.cone_axis[2],
                    meshletBounds.cone_cutoff),
            });
    }

    for (const auto& vertexIndex : geometry.vertices) {
        CRISP_CHECK_GE_LT(vertexIndex, 0u, static_cast<uint32_t>(vertexCount));
    }

    return geometry;
}

bool isMeshletConeVisible(const MeshletBounds& bounds, const glm::vec3& cameraPosition) {
    const float coneCutoff = bounds.coneAxisCutoff.w;
    if (coneCutoff >= 1.0f) {
        return true;
    }

    const glm::vec3 apex{bounds.coneApex};
    const glm::vec3 axis{bounds.coneAxisCutoff};
    const glm::vec3 toApex{apex - cameraPosition};
    const float distanceSquared = glm::dot(toApex, toApex);
    if (distanceSquared <= 0.0f) {
        return true;
    }

    return glm::dot(toApex, axis) < coneCutoff * std::sqrt(distanceSquared);
}

} // namespace crisp
