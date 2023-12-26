#pragma once

#include <Crisp/Image/Image.hpp>
#include <Crisp/Math/Headers.hpp>

#include <array>
#include <optional>
#include <string>

namespace crisp {
struct PbrParams {
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 uvScale{1.0f, 1.0f};
    float metallic{1.0f};
    float roughness{1.0f};
    float aoStrength{1.0f};
};

struct PbrTextureGroup {
    std::optional<Image> albedo;
    std::optional<Image> normal;
    std::optional<Image> roughness;
    std::optional<Image> metallic;
    std::optional<Image> occlusion;
    std::optional<Image> emissive;
};

struct PbrMaterial {
    std::string name;
    PbrTextureGroup textures;
    PbrParams params;
};

Image createDefaultAlbedoMap(const std::array<uint8_t, 4>& color = {255, 0, 255, 255});
Image createDefaultNormalMap();
Image createDefaultMetallicMap(float metallic = 0.0f);
Image createDefaultRoughnessMap(float roughness = 1.0f);
Image createDefaultAmbientOcclusionMap();
Image createDefaultEmissiveMap();

PbrTextureGroup createDefaultPbrTextureGroup();

} // namespace crisp
