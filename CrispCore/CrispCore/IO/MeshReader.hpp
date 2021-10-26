#pragma once

#include <CrispCore/Mesh/TriangleMesh.hpp>

#include <filesystem>

namespace crisp
{
    TriangleMesh loadMesh(const std::filesystem::path& path,
        const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
            VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord });
}
