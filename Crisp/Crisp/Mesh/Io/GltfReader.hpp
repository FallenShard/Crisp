#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <filesystem>
#include <optional>

namespace crisp
{

Result<std::pair<TriangleMesh, PbrTextureGroup>> loadGltfModel(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes);

} // namespace crisp
