#pragma once

#include <vector>

#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Optics/Fresnel.hpp>

namespace crisp {
struct BrdfParameters {
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    int32_t type;

    float intIor{Fresnel::getIOR(IndexOfRefraction::Glass)};
    float extIor{Fresnel::getIOR(IndexOfRefraction::Vacuum)};
    int32_t lobe;
    int32_t microfacetType;

    glm::vec3 kd;
    float ks;

    glm::vec3 complexIorEta;
    float microfacetAlpha;

    glm::vec3 complexIorK;
    float pad1;
};

struct InstanceProperties {
    int32_t materialId{-1};
    int32_t lightId{-1};
    uint32_t vertexOffset{0};
    uint32_t triangleOffset{0};
    uint32_t aliasTableOffset{0};
    uint32_t aliasTableCount{0};
    uint32_t triangleCount{0};
    uint32_t pad1{};
};

struct LightParameters {
    int32_t type;
    int32_t meshId{-1};
    int32_t pad0{};
    int32_t pad1{};
    glm::vec3 radiance;
    float pad2{};
};

struct SceneDescription {
    std::vector<std::string> meshFilenames;
    std::vector<glm::mat4> transforms;
    std::vector<InstanceProperties> props;
    std::vector<BrdfParameters> brdfs;
    std::vector<LightParameters> lights;
};

glm::vec3 parseVec3(const nlohmann::json& json);

BrdfParameters createMicrofacetBrdf(glm::vec3 kd, float alpha);

SceneDescription parseSceneDescription(const nlohmann::json& shapeList);

} // namespace crisp