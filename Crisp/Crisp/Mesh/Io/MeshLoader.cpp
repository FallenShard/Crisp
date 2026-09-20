#include <Crisp/Mesh/Io/MeshLoader.hpp>

#include <span>

#include <meshoptimizer.h>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/MeshOptimizer.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("MeshLoader");

struct TriangleMeshlets {
    TriangleMesh mesh;
    MeshletData meshlets;
};

TriangleMesh convertToTriangleMesh(
    const std::filesystem::path& path,
    WavefrontObjMesh&& objMesh, // NOLINT
    const TriangleMeshLoadOptions& options) {
    TriangleMesh mesh(
        std::move(objMesh.positions),
        std::move(objMesh.normals),
        std::move(objMesh.texCoords),
        std::move(objMesh.triangles));

    if (mesh.getNormals().empty() && options.computeVertexNormals) {
        mesh.computeVertexNormals();
    }
    if (mesh.getTangents().empty() && options.computeTangents) {
        mesh.computeTangentVectors();
    }

    mesh.setMeshName(path.filename().stem().string());
    mesh.setViews(std::move(objMesh.views));

    if (options.optimizeIndices) {
        const auto stats = optimizeMeshIndices(mesh);
        logger->info(
            "Optimized {}: {} -> {} vertices, ATVR {:.3f} -> {:.3f}, overfetch {:.3f} -> {:.3f}.",
            mesh.getMeshName(),
            stats.vertexCountBefore,
            stats.vertexCountAfter,
            stats.atvrBefore,
            stats.atvrAfter,
            stats.overfetchBefore,
            stats.overfetchAfter);
    }

    return mesh;
}

TriangleMeshlets convertToTriangleMeshlets(
    const std::filesystem::path& path, WavefrontObjMesh&& objMesh, const TriangleMeshLoadOptions& options) { // NOLINT
    auto mesh = convertToTriangleMesh(path, std::move(objMesh), options);

    const size_t max_vertices = 64;
    const size_t max_triangles = 124;
    const float cone_weight = 0.0f;

    MeshletData meshletData{};
    meshletData.maxVertices = max_vertices;
    meshletData.maxTriangles = max_triangles;
    meshletData.maxMeshletCount =
        static_cast<uint32_t>(meshopt_buildMeshletsBound(mesh.getIndexCount(), max_vertices, max_triangles));

    std::vector<meshopt_Meshlet> builtMeshlets(meshletData.maxMeshletCount);
    meshletData.meshletVertices.resize(meshletData.maxMeshletCount * max_vertices);
    meshletData.meshletTriangles.resize(meshletData.maxMeshletCount * max_triangles * 3);

    const size_t meshletCount = meshopt_buildMeshlets(
        builtMeshlets.data(),
        meshletData.meshletVertices.data(),
        meshletData.meshletTriangles.data(),
        mesh.getIndices(),
        mesh.getIndexCount(),
        glm::value_ptr(mesh.getPositions()[0]),
        mesh.getVertexCount(),
        sizeof(glm::vec3),
        max_vertices,
        max_triangles,
        cone_weight);
    const meshopt_Meshlet& last = builtMeshlets[meshletCount - 1];

    meshletData.meshletVertices.resize(last.vertex_offset + last.vertex_count);
    meshletData.meshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));

    meshletData.meshlets.reserve(meshletCount);
    for (const auto& built : std::span{builtMeshlets}.first(meshletCount)) {
        meshletData.meshlets.push_back(
            Meshlet{
                .vertexOffset = built.vertex_offset,
                .triangleOffset = built.triangle_offset,
                .vertexCount = built.vertex_count,
                .triangleCount = built.triangle_count,
            });
    }

    for (const auto& vertexIndex : meshletData.meshletVertices) {
        CRISP_CHECK_GE_LT(vertexIndex, 0, mesh.getVertexCount());
    }

    return {std::move(mesh), std::move(meshletData)};
}
} // namespace

Result<TriangleMesh> loadTriangleMesh(const std::filesystem::path& path, const TriangleMeshLoadOptions& options) {
    if (isWavefrontObjFile(path)) {
        return convertToTriangleMesh(path, loadWavefrontObj(path), options);
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<MeshAndMaterial> loadTriangleMeshAndMaterial( // NOLINT
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options) {
    if (isWavefrontObjFile(path)) {
        auto objMesh = loadWavefrontObj(path);
        auto materials = std::move(objMesh.materials);
        return MeshAndMaterial{convertToTriangleMesh(path, std::move(objMesh), options), std::move(materials)};
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options) {
    if (isWavefrontObjFile(path)) {
        auto objMesh = loadWavefrontObj(path);
        auto materials = std::move(objMesh.materials);
        auto [mesh, meshlets] = convertToTriangleMeshlets(path, std::move(objMesh), options);
        return MeshAndMaterialMeshlets{std::move(mesh), std::move(materials), std::move(meshlets)};
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

} // namespace crisp