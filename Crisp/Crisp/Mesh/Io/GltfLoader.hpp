#pragma once

#include <Crisp/Common/Result.hpp>
#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

#include <filesystem>

namespace crisp
{

struct ModelData
{
    TriangleMesh mesh;
    PbrMaterial material;

    glm::mat4 transform;

    SkinningData skinningData;

    std::vector<GltfAnimation> animations;
};

Result<std::vector<ModelData>> loadGltfModel(
    const std::filesystem::path& path, const std::vector<VertexAttributeDescriptor>& vertexAttributes = {});

} // namespace crisp
