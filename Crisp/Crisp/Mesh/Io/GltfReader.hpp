#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <filesystem>

namespace crisp
{

struct RenderObject
{
    TriangleMesh mesh;
    PbrMaterial material;
    glm::mat4 transform;

    SkinningData skinningData;
};

Result<std::vector<RenderObject>> loadGltfModel(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes);

} // namespace crisp
