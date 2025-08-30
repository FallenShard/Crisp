#include <Crisp/Mesh/Io/MeshLoader.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>

#include <meshoptimizer.h>

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
    const MeshLoadOptions& options) {
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
    return mesh;
}

TriangleMeshlets convertToTriangleMeshlets(
    const std::filesystem::path& path, WavefrontObjMesh&& objMesh, const MeshLoadOptions& options) {
    auto mesh = convertToTriangleMesh(path, std::move(objMesh), options);

    // const float threshold = 0.2f;
    // size_t targetIndexCount = static_cast<uint32_t>(static_cast<float>(mesh.getIndexCount()) * threshold);
    // const float targetError = 1e-2f;

    // std::vector<uint32_t> lod(mesh.getIndexCount());

    // const auto count = meshopt_simplify(
    //     lod.data(),
    //     mesh.getIndices(),
    //     mesh.getIndexCount(),
    //     mesh.getPositionsPtr(),
    //     mesh.getVertexCount(),
    //     sizeof(glm::vec3),
    //     targetIndexCount,
    //     targetError);

    const size_t max_vertices = 64;
    const size_t max_triangles = 124;
    const float cone_weight = 0.0f;

    MeshletData meshletData{};
    meshletData.maxVertices = max_vertices;
    meshletData.maxTriangles = max_triangles;
    meshletData.maxMeshletCount = meshopt_buildMeshletsBound(mesh.getIndexCount(), max_vertices, max_triangles);

    meshletData.meshlets.resize(meshletData.maxMeshletCount);
    meshletData.meshletVertices.resize(meshletData.maxMeshletCount * max_vertices);
    meshletData.meshletTriangles.resize(meshletData.maxMeshletCount * max_triangles * 3);

    const size_t meshletCount = meshopt_buildMeshlets(
        meshletData.meshlets.data(),
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
    const meshopt_Meshlet& last = meshletData.meshlets[meshletCount - 1];

    meshletData.meshletVertices.resize(last.vertex_offset + last.vertex_count);
    meshletData.meshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshletData.meshlets.resize(meshletCount);

    for (const auto& vertexIndex : meshletData.meshletVertices) {
        CRISP_CHECK_GE_LT(vertexIndex, 0, mesh.getVertexCount());
    }

    return {std::move(mesh), std::move(meshletData)};
}
} // namespace

Result<TriangleMesh> loadTriangleMesh(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes) { // NOLINT
    if (isWavefrontObjFile(path)) {
        return convertToTriangleMesh(path, loadWavefrontObj(path), {});
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<TriangleMesh> loadTriangleMesh(const std::filesystem::path& path, const MeshLoadOptions& options) {
    if (isWavefrontObjFile(path)) {
        return convertToTriangleMesh(path, loadWavefrontObj(path), options);
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes) { // NOLINT
    if (isWavefrontObjFile(path)) {
        auto objMesh = loadWavefrontObj(path);
        auto materials = std::move(objMesh.materials);
        return MeshAndMaterial{convertToTriangleMesh(path, std::move(objMesh), {}), std::move(materials)};
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(const std::filesystem::path& path, const MeshLoadOptions& options) {
    if (isWavefrontObjFile(path)) {
        auto objMesh = loadWavefrontObj(path);
        auto materials = std::move(objMesh.materials);
        auto [mesh, meshlets] = convertToTriangleMeshlets(path, std::move(objMesh), options);
        return MeshAndMaterialMeshlets{std::move(mesh), std::move(materials), std::move(meshlets)};
    }

    return resultError("Failed to open an obj mesh at {}", path.string());
}

} // namespace crisp