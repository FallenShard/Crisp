#include <Crisp/Materials/PbrMaterial.hpp>

namespace crisp {
Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color) {
    return {std::vector<uint8_t>(color.begin(), color.end()), 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createDefaultNormalMap() {
    return {std::vector<uint8_t>{128, 128, 255, 255}, 1, 1, 4, 4 * sizeof(uint8_t)};
}

Image createDefaultMetallicMap(const float metallic) {
    return {std::vector<uint8_t>{static_cast<uint8_t>(metallic * 255.0f)}, 1, 1, 1, 1 * sizeof(uint8_t)};
}

Image createDefaultRoughnessMap(const float roughness) {
    return {std::vector<uint8_t>{static_cast<uint8_t>(roughness * 255.0f)}, 1, 1, 1, 1 * sizeof(uint8_t)};
}

Image createDefaultAmbientOcclusionMap() {
    return {std::vector<uint8_t>{255}, 1, 1, 1, 1 * sizeof(uint8_t)};
}

Image createDefaultEmissiveMap() {
    return {std::vector<uint8_t>{0, 0, 0, 0}, 1, 1, 4, 4 * sizeof(uint8_t)};
}

PbrImageGroup createDefaultPbrImageGroup() {
    return {
        .name = "default",
        .albedoMaps = {createDefaultAlbedoMap(std::array<uint8_t, 4>{255, 255, 255, 255})},
        .normalMaps = {createDefaultNormalMap()},
        .roughnessMaps = {createDefaultRoughnessMap()},
        .metallicMaps = {createDefaultMetallicMap()},
        .occlusionMaps = {createDefaultAmbientOcclusionMap()},
        .emissiveMaps = {createDefaultEmissiveMap()},
    };
}
} // namespace crisp
