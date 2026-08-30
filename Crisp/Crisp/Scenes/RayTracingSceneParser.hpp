#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/PathTracer/Optics/Fresnel.hpp>

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
    float roughness;

    int32_t reflectanceTexture{-1};
    int32_t reflectanceSampler{-1};
    int32_t pad0{};
    int32_t pad1{};
};

static_assert(sizeof(BrdfParameters) == 96);
static_assert(std::is_standard_layout_v<BrdfParameters>);
static_assert(offsetof(BrdfParameters, albedo) == 0);
static_assert(offsetof(BrdfParameters, kd) == 32);
static_assert(offsetof(BrdfParameters, complexIorEta) == 48);
static_assert(offsetof(BrdfParameters, complexIorK) == 64);
static_assert(offsetof(BrdfParameters, reflectanceTexture) == 80);

struct MaterialTextureDescription {
    std::string filename;
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

struct EnvironmentLightDescription {
    std::optional<std::string> filename;
    std::optional<glm::vec3> radiance;
    float radianceScale{1.0f};
};

struct SceneDescription {
    std::vector<std::string> meshFilenames;
    std::vector<glm::mat4> transforms;
    std::vector<InstanceProperties> props;
    std::vector<BrdfParameters> brdfs;
    std::vector<MaterialTextureDescription> materialTextures;
    std::vector<LightParameters> lights;
    std::optional<EnvironmentLightDescription> environment;
};

Result<glm::vec3> parseVec3(const nlohmann::json& json);

BrdfParameters createMicrofacetBrdf(glm::vec3 kd, float alpha, int32_t microfacetType = 0);

Result<SceneDescription> parseSceneDescription(const nlohmann::json& shapeList, const nlohmann::json& lightList = {});

} // namespace crisp
