#pragma once

#include <bitset>

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

struct ImageData {
    Image image;
    std::string name;

    std::bitset<6> accessTypes{};

    bool hasSingleAccessType() const {
        return accessTypes.count() == 1;
    }

    bool hasTwoAccessTypes() const {
        return accessTypes.count() == 2;
    }
};

struct SceneData {
    PbrImageGroup images;
    std::vector<ModelData> models;
};

} // namespace crisp