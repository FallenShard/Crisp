#include <Crisp/Mesh/Io/MeshLoader.hpp>

#include <Crisp/Core/Checks.hpp>
#include <Crisp/Core/Logger.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/MeshOptimizer.hpp>
#include <Crisp/Mesh/MeshletBuilder.hpp>

namespace crisp {
namespace {
auto logger = createLoggerMt("MeshLoader");

struct TriangleMeshlets {
    TriangleMesh mesh;
    MeshletGeometry meshlets;
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
    auto meshlets = buildMeshlets(mesh);
    logger->info(
        "Built {} meshlets for {} ({} triangles).",
        meshlets.meshlets.size(),
        mesh.getMeshName(),
        mesh.getTriangleCount());
    return {std::move(mesh), std::move(meshlets)};
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