#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <type_traits>

#include <Crisp/Core/Format.hpp>
#include <Crisp/Image/Image.hpp>
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

// Mirrors PbrMaterialParameters in Shaders/pbr.frag.glsl.
// 0 is the registry's fallback, so an unauthored map samples the checkerboard.
// Targets the supported opaque subset of OpenPBR Surface 1.1.1:
// https://academysoftwarefoundation.github.io/OpenPBR/
struct PbrMaterialParams {
    glm::vec3 baseColor{0.8f, 0.8f, 0.8f};
    float baseWeight{1.0f};

    glm::vec3 specularColor{1.0f, 1.0f, 1.0f};
    float specularWeight{1.0f};

    glm::vec3 emissionColor{1.0f, 1.0f, 1.0f};
    float emissionLuminance{0.0f};

    glm::vec2 uvScale{1.0f, 1.0f};
    float baseMetalness{0.0f};
    float baseDiffuseRoughness{0.0f};

    float specularRoughness{0.3f};
    float specularIor{1.5f};
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
};

static_assert(sizeof(PbrMaterialParams) == 112);
static_assert(std::is_standard_layout_v<PbrMaterialParams>);
static_assert(offsetof(PbrMaterialParams, baseColor) == 0);
static_assert(offsetof(PbrMaterialParams, baseWeight) == 12);
static_assert(offsetof(PbrMaterialParams, specularColor) == 16);
static_assert(offsetof(PbrMaterialParams, specularWeight) == 28);
static_assert(offsetof(PbrMaterialParams, emissionColor) == 32);
static_assert(offsetof(PbrMaterialParams, emissionLuminance) == 44);
static_assert(offsetof(PbrMaterialParams, uvScale) == 48);
static_assert(offsetof(PbrMaterialParams, baseMetalness) == 56);
static_assert(offsetof(PbrMaterialParams, baseDiffuseRoughness) == 60);
static_assert(offsetof(PbrMaterialParams, specularRoughness) == 64);
static_assert(offsetof(PbrMaterialParams, specularIor) == 68);
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

} // namespace crisp
