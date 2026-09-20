#pragma once

#include <filesystem>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>
#include <Crisp/Mesh/Io/MeshLoadOptions.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/Meshlet.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MeshletData {
    uint32_t maxMeshletCount{0};
    uint32_t maxVertices{0};
    uint32_t maxTriangles{0};

    std::vector<Meshlet> meshlets;
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

Result<TriangleMesh> loadTriangleMesh(const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

} // namespace crisp
