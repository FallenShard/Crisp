#pragma once

#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {
struct ModelData {
    TriangleMesh mesh;

    PbrMaterial material;

    glm::mat4 transform;

    SkinningData skinningData;

    std::vector<AnimationData> animations;
};

struct TextureData {
    enum class Format { sRGB, Linear };

    Format format = Format::Linear;
    Image image;
};

struct SceneData {
    std::vector<TextureData> textures;
    std::vector<ModelData> models;
};

} // namespace crisp