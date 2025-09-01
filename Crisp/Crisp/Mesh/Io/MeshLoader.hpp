#pragma once

#include <filesystem>

#include <meshoptimizer.h>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MeshletData {
    uint32_t maxMeshletCount{0};
    uint32_t maxVertices{0};
    uint32_t maxTriangles{0};

    std::vector<meshopt_Meshlet> meshlets;
    std::vector<unsigned int> meshletVertices;
    std::vector<unsigned char> meshletTriangles;
};

struct MeshAndMaterial {
    TriangleMesh mesh;
    FlatHashMap<std::string, WavefrontObjMaterial> materials;
};

struct MeshAndMaterialMeshlets {
    TriangleMesh mesh;
    FlatHashMap<std::string, WavefrontObjMaterial> materials;

    MeshletData meshlets;
};

struct TriangleMeshLoadOptions {
    bool normalizeNormals{true};     // If normals are available, they will be renormalized.
    bool computeVertexNormals{true}; // If normals are not available, they will be computed on load.
    bool computeTangents{true};      // If tangents are not available, they will be computed on load.
};

Result<TriangleMesh> loadTriangleMesh(const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(const std::filesystem::path& path);

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

} // namespace crisp
