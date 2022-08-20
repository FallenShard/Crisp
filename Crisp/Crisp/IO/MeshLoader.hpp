#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>


#include <filesystem>

namespace crisp
{
Result<TriangleMesh> loadTriangleMesh(
    const std::filesystem::path& path,
    const std::vector<VertexAttributeDescriptor>& vertexAttributes = {
        VertexAttribute::Position, VertexAttribute::Normal, VertexAttribute::TexCoord});
}
