#pragma once

#include <filesystem>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>
#include <Crisp/Mesh/Io/MeshLoadOptions.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/MeshletBuilder.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MeshAndMaterial {
    TriangleMesh mesh;
    FlatHashMap<std::string, WavefrontObjMaterial> materials;
};

struct MeshAndMaterialMeshlets {
    TriangleMesh mesh;
    FlatHashMap<std::string, WavefrontObjMaterial> materials;

    MeshletGeometry meshlets;
};

Result<TriangleMesh> loadTriangleMesh(const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(
    const std::filesystem::path& path, const TriangleMeshLoadOptions& options = {});

} // namespace crisp
