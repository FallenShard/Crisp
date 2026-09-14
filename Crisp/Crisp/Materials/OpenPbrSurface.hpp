#pragma once

#include <cstddef>
#include <type_traits>

#include <Crisp/Math/Headers.hpp>

namespace crisp {

// The supported opaque subset of OpenPBR Surface 1.1.1, and nothing else:
// https://academysoftwarefoundation.github.io/OpenPBR/
//
// This is the one parameter vocabulary shared by the rasterizer and both path tracers. PbrMaterialParams
// embeds it alongside Crisp's renderer extensions (uv scale, texture indices, alpha masking); PbrMaterialParams
// uses it for kBsdfOpenPbr and as canonical storage for equivalent legacy values. Two evaluators of different
// fidelity read the same record, and this is the single place its ABI is asserted.
//
// Nothing that the renderer needs but OpenPBR does not define belongs here -- that is what keeps the block
// usable as a deprecation seam once the legacy BSDF types go.
//
// Mirrors OpenPbrSurfaceParams in Shaders/Common/openpbr-surface.part.glsl. The field order keeps every vec3
// followed by a float, so the block measures 64 bytes under scalar and std430 alike -- the rasterizer reads it
// through an std430 buffer reference, the path tracers through a scalar one.
struct OpenPbrSurfaceParams {
    glm::vec3 baseColor{0.8f, 0.8f, 0.8f};
    float baseWeight{1.0f};

    glm::vec3 specularColor{1.0f, 1.0f, 1.0f};
    float specularWeight{1.0f};

    glm::vec3 emissionColor{1.0f, 1.0f, 1.0f};
    float emissionLuminance{0.0f};

    float baseMetalness{0.0f};
    float baseDiffuseRoughness{0.0f};
    float specularRoughness{0.3f};
    float specularIor{1.5f};
};

static_assert(sizeof(OpenPbrSurfaceParams) == 64);
static_assert(std::is_standard_layout_v<OpenPbrSurfaceParams>);
static_assert(offsetof(OpenPbrSurfaceParams, baseColor) == 0);
static_assert(offsetof(OpenPbrSurfaceParams, baseWeight) == 12);
static_assert(offsetof(OpenPbrSurfaceParams, specularColor) == 16);
static_assert(offsetof(OpenPbrSurfaceParams, specularWeight) == 28);
static_assert(offsetof(OpenPbrSurfaceParams, emissionColor) == 32);
static_assert(offsetof(OpenPbrSurfaceParams, emissionLuminance) == 44);
static_assert(offsetof(OpenPbrSurfaceParams, baseMetalness) == 48);
static_assert(offsetof(OpenPbrSurfaceParams, baseDiffuseRoughness) == 52);
static_assert(offsetof(OpenPbrSurfaceParams, specularRoughness) == 56);
static_assert(offsetof(OpenPbrSurfaceParams, specularIor) == 60);

} // namespace crisp
