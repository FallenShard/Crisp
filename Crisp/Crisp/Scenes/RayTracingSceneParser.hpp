#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include <Crisp/Core/Result.hpp>
#include <Crisp/Io/JsonUtils.hpp>
#include <Crisp/Materials/Ior.hpp>
#include <Crisp/Materials/OpenPbrSurface.hpp>
#include <Crisp/Math/Headers.hpp>
#include <Crisp/Scenes/PathTracer.hpp>
#include <Crisp/Vulkan/Rhi/VulkanHeader.hpp>

namespace crisp {
enum class ReconstructionFilterType : int32_t { // NOLINT
    Box = 0,
};

struct RayTracingRenderSettings {
    glm::ivec2 resolution{1920, 1080};
    uint32_t seed{0};
    int32_t samplesPerPixel{64};
    int32_t maxDepth{32};
    int32_t samplingMode{0};
    ReconstructionFilterType reconstructionFilter{ReconstructionFilterType::Box};

    glm::vec3 cameraPosition{0.0f, 1.0f, 10.0f};
    glm::vec3 cameraTarget{0.0f, 1.0f, 9.0f};
    glm::vec3 cameraUp{0.0f, 1.0f, 0.0f};
    float verticalFov{45.0f};
    float zNear{0.1f};
    float zFar{1000.0f};
};

struct BsdfParameters {
    OpenPbrSurfaceParams surface;

    glm::vec3 complexIorEta;
    float microfacetAlpha;

    glm::vec3 complexIorK;
    float orenNayarRoughness;

    int32_t type;
    int32_t microfacetType;
    int32_t reflectanceTexture{-1};
    int32_t reflectanceSampler{-1};
};

static_assert(sizeof(BsdfParameters) == 112);
static_assert(std::is_standard_layout_v<BsdfParameters>);
static_assert(offsetof(BsdfParameters, surface) == 0);
static_assert(offsetof(BsdfParameters, complexIorEta) == 64);
static_assert(offsetof(BsdfParameters, complexIorK) == 80);
static_assert(offsetof(BsdfParameters, type) == 96);
static_assert(offsetof(BsdfParameters, reflectanceTexture) == 104);

struct MaterialTextureDescription {
    std::string filename;
};

inline constexpr int32_t kLightArea = 0;
inline constexpr int32_t kLightPoint = 1;
inline constexpr int32_t kLightDirectional = 2;

struct LightParameters {
    int32_t type{kLightArea};
    int32_t meshId{-1};
    int32_t pad0{};
    int32_t pad1{};
    glm::vec3 emission{}; // Area radiance, point power, or directional irradiance.
    float pad2{};
    glm::vec3 positionOrDirection{};
    float pad3{};
};

static_assert(sizeof(LightParameters) == 48);
static_assert(std::is_standard_layout_v<LightParameters>);
static_assert(offsetof(LightParameters, type) == 0);
static_assert(offsetof(LightParameters, emission) == 16);
static_assert(offsetof(LightParameters, positionOrDirection) == 32);

struct EnvironmentLightDescription {
    std::optional<std::string> filename;
    std::optional<glm::vec3> radiance;
    float radianceScale{1.0f};
};

struct SceneDescription {
    std::vector<std::string> meshFilenames;
    std::vector<glm::mat4> transforms;
    std::vector<PathTracedInstance> props;
    std::vector<BsdfParameters> bsdfs;
    std::vector<MaterialTextureDescription> materialTextures;
    std::vector<LightParameters> lights;
    std::optional<EnvironmentLightDescription> environment;
};

Result<glm::vec3> parseVec3(const nlohmann::json& json);
Result<RayTracingRenderSettings> parseRayTracingRenderSettings(const nlohmann::json& json);

BsdfParameters createMicrofacetBsdf(glm::vec3 kd, float alpha, int32_t microfacetType = 0);

Result<SceneDescription> parseSceneDescription(const nlohmann::json& shapeList, const nlohmann::json& lightList = {});

} // namespace crisp
