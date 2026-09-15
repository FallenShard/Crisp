#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <type_traits>

#include <Crisp/Core/Format.hpp>
#include <Crisp/Image/Image.hpp>
#include <Crisp/Materials/OpenPbrSurface.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp {

inline constexpr uint32_t kPbrAlbedoMapIndex = 0;
inline constexpr uint32_t kPbrNormalMapIndex = 1;
inline constexpr uint32_t kPbrOrmMapIndex = 2;
inline constexpr uint32_t kPbrEmissiveMapIndex = 3;
inline constexpr uint32_t kPbrMapTypeCount = 4;

enum PbrMaterialFlagBits : uint32_t {
    PbrMaterialAlphaMask = 1u << 0,
    PbrMaterialDoubleSided = 1u << 1,
};

inline constexpr std::array<std::string_view, kPbrMapTypeCount> kPbrMapNames = {
    "albedo",
    "normal",
    "orm",
    "emissive",
};

inline constexpr float kMinSpecularRoughness = 1e-3f;

inline constexpr int32_t kBsdfLambertian = 0;
inline constexpr int32_t kBsdfDielectric = 1;
inline constexpr int32_t kBsdfMirror = 2;
inline constexpr int32_t kBsdfMicrofacet = 3;
inline constexpr int32_t kBsdfOrenNayar = 4;
inline constexpr int32_t kBsdfSmoothConductor = 5;
inline constexpr int32_t kBsdfRoughConductor = 6;
inline constexpr int32_t kBsdfRoughDielectric = 7;
inline constexpr int32_t kBsdfOpenPbr = 8;

// Mirrors PbrMaterialParameters in Shaders/pbr.frag.glsl, Shaders/pbr-heap.frag.glsl and
// Shaders/PathTracer/Core/pbr-scene.part.glsl.
//
// The OpenPBR half is one contiguous named block so the rasterizer and the path tracers all
// read the same record; everything after it is a Crisp renderer extension, not an OpenPBR parameter.
// 0 is the registry's fallback, so an unauthored map samples the checkerboard.
struct PbrMaterialParams {
    OpenPbrSurfaceParams surface;

    glm::vec2 uvScale{1.0f, 1.0f};
    float normalScale{1.0f};
    float aoStrength{1.0f};

    uint32_t samplerIndex{0};
    uint32_t baseColorTex{0};
    uint32_t normalTex{0};
    uint32_t ormTex{0};

    uint32_t emissionTex{0};
    float geometryOpacity{1.0f};
    float alphaCutoff{0.5f};
    uint32_t flags{0};

    // Per-lobe parameters, selected by `type`. Anything with an exact OpenPBR equivalent lives in `surface`
    // instead, so only values with no shared representation appear here; exactly one lobe's fields are live at
    // a time. The rasteriser reads nothing past `flags`, but must still declare the full record: the struct's
    // size is the array stride.
    glm::vec3 complexIorEta{0.0f};
    float orenNayarRoughness{0.0f};

    glm::vec3 complexIorK{0.0f};
    int32_t type{kBsdfOpenPbr};

    int32_t microfacetType{0};
    uint32_t pad0{0};
    uint32_t pad1{0};
    uint32_t pad2{0};
};

static_assert(sizeof(PbrMaterialParams) == 160);
static_assert(offsetof(PbrMaterialParams, complexIorEta) == 112);
static_assert(offsetof(PbrMaterialParams, complexIorK) == 128);
static_assert(offsetof(PbrMaterialParams, type) == 140);
static_assert(offsetof(PbrMaterialParams, microfacetType) == 144);
static_assert(std::is_standard_layout_v<PbrMaterialParams>);
static_assert(offsetof(PbrMaterialParams, surface) == 0);
static_assert(offsetof(PbrMaterialParams, uvScale) == 64);
static_assert(offsetof(PbrMaterialParams, normalScale) == 72);
static_assert(offsetof(PbrMaterialParams, aoStrength) == 76);
static_assert(offsetof(PbrMaterialParams, samplerIndex) == 80);
static_assert(offsetof(PbrMaterialParams, baseColorTex) == 84);
static_assert(offsetof(PbrMaterialParams, normalTex) == 88);
static_assert(offsetof(PbrMaterialParams, ormTex) == 92);
static_assert(offsetof(PbrMaterialParams, emissionTex) == 96);
static_assert(offsetof(PbrMaterialParams, geometryOpacity) == 100);
static_assert(offsetof(PbrMaterialParams, alphaCutoff) == 104);
static_assert(offsetof(PbrMaterialParams, flags) == 108);

struct PbrImageKeyCreator {
    std::string name;

    std::string createMapKey(const uint32_t MapTypeIndex, const uint32_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[MapTypeIndex], index);
    }

    std::string createAlbedoMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrAlbedoMapIndex], index);
    }

    std::string createNormalMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrNormalMapIndex], index);
    }

    std::string createOrmMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrOrmMapIndex], index);
    }

    std::string createEmissiveMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrEmissiveMapIndex], index);
    }
};

struct PbrImageGroup {
    std::string name;

    std::vector<Image> albedoMaps;
    std::vector<Image> normalMaps;
    std::vector<Image> ormMaps;
    std::vector<Image> emissiveMaps;

    std::size_t size() const {
        return albedoMaps.size() + normalMaps.size() + ormMaps.size() + emissiveMaps.size();
    }

    PbrImageKeyCreator createKeyCreator() const {
        return {name};
    }
};

struct PbrMaterial {
    std::string name;
    PbrMaterialParams params;
    std::array<std::string, kPbrMapTypeCount> textureKeys;
};

Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color = {255, 0, 255, 255});
Image createDefaultNormalMap();
Image createDefaultOrmMap();
Image createDefaultEmissiveMap();

struct PbrOrmSources {
    const Image* occlusion{nullptr};
    uint32_t occlusionChannel{0};
    const Image* roughness{nullptr};
    uint32_t roughnessChannel{0};
    const Image* metallic{nullptr};
    uint32_t metallicChannel{0};
};

Image createPbrOrmMap(const PbrOrmSources& sources);

PbrImageGroup createDefaultPbrImageGroup();

inline void clampToGpuRange(PbrMaterialParams& params) {
    params.surface.specularRoughness = std::clamp(params.surface.specularRoughness, kMinSpecularRoughness, 1.0f);
}

} // namespace crisp
