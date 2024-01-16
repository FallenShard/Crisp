#pragma once

#include <array>
#include <string>

#include <Crisp/Core/Format.hpp>
#include <Crisp/Image/Image.hpp>
#include <Crisp/Math/Headers.hpp>

namespace crisp {

inline constexpr uint32_t kPbrAlbedoMapIndex = 0;
inline constexpr uint32_t kPbrNormalMapIndex = 1;
inline constexpr uint32_t kPbrRoughnessMapIndex = 2;
inline constexpr uint32_t kPbrMetallicMapIndex = 3;
inline constexpr uint32_t kPbrOcclusionMapIndex = 4;
inline constexpr uint32_t kPbrEmissiveMapIndex = 5;
inline constexpr uint32_t kPbrMapTypeCount = 6;

inline constexpr std::array<std::string_view, kPbrMapTypeCount> kPbrMapNames = {
    "albedo",
    "normal",
    "metallic",
    "roughness",
    "occlusion",
    "emissive",
};

struct PbrParams {
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 uvScale{1.0f, 1.0f};
    float metallic{1.0f};
    float roughness{1.0f};
    float aoStrength{1.0f};
};

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

    std::string createRoughnessMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrRoughnessMapIndex], index);
    }

    std::string createMetallicMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrMetallicMapIndex], index);
    }

    std::string createOcclusionMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrOcclusionMapIndex], index);
    }

    std::string createEmissiveMapKey(const size_t index) const {
        return fmt::format("{}-{}-{}", name, kPbrMapNames[kPbrEmissiveMapIndex], index);
    }
};

struct PbrImageGroup {
    std::string name;

    std::vector<Image> albedoMaps;
    std::vector<Image> normalMaps;
    std::vector<Image> roughnessMaps;
    std::vector<Image> metallicMaps;
    std::vector<Image> occlusionMaps;
    std::vector<Image> emissiveMaps;

    std::size_t size() const {
        return albedoMaps.size() + normalMaps.size() + roughnessMaps.size() + metallicMaps.size() +
               occlusionMaps.size() + emissiveMaps.size();
    }

    PbrImageKeyCreator createKeyCreator() const {
        return {name};
    }
};

struct PbrMaterial {
    std::string name;
    PbrParams params;
    std::array<std::string, kPbrMapTypeCount> textureKeys;
};

Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color = {255, 0, 255, 255});
Image createDefaultNormalMap();
Image createDefaultMetallicMap(float metallic = 0.0f);
Image createDefaultRoughnessMap(float roughness = 1.0f);
Image createDefaultAmbientOcclusionMap();
Image createDefaultEmissiveMap();

PbrImageGroup createDefaultPbrImageGroup();

} // namespace crisp
