#pragma once

#include <bitset>

#include <Crisp/Materials/PbrMaterial.hpp>
#include <Crisp/Mesh/SkinningData.hpp>
#include <Crisp/Mesh/TriangleMesh.hpp>

namespace crisp {

struct MaterialData {
    PbrMaterial pbr;
    std::array<int32_t, 6> textureIndices;
};

struct ModelData {
    TriangleMesh mesh;

    MaterialData material;

    glm::mat4 transform;

    SkinningData skinningData;

    std::vector<AnimationData> animations;
};

struct ImageData {
    Image image;
    std::string name;

    std::bitset<6> accessTypes{};

    inline bool hasSingleAccessType() const {
        return accessTypes.count() == 1;
    }
};

struct PbrImageData {
    std::vector<Image> albedoMaps;
    std::vector<Image> normalMaps;
    std::vector<Image> roughnessMaps;
    std::vector<Image> metallicMaps;
    std::vector<Image> occlusionMaps;
    std::vector<Image> emissiveMaps;
};

struct SceneData {
    PbrImageData images;
    std::vector<ModelData> models;
};

} // namespace crisp