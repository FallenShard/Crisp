#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Mesh/Io/GltfLoader.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <filesystem>

namespace crisp
{

struct MeshAndMaterial
{
    TriangleMesh mesh;
    FlatHashMap<std::string, WavefrontObjMaterial> materials;
};

Result<TriangleMesh> loadTriangleMesh(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});

Result<MeshAndMaterial> loadTriangleMeshAndMaterial(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});

} // namespace crisp
