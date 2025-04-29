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

Result<TriangleMesh> loadTriangleMesh(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});

Result<MeshAndMaterialMeshlets> loadTriangleMeshlets(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});

} // namespace crisp
