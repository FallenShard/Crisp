#ifndef CRISP_PBR_MATERIAL_GLSL
#define CRISP_PBR_MATERIAL_GLSL

#include "openpbr-surface.part.glsl"

// The one material record. Must match PbrMaterialParams in Materials/PbrMaterial.hpp.
//
// It lives here rather than in each consumer because the struct's size is the array stride: a copy that drifts
// does not fail to compile, it silently reads the wrong material for every index past zero. The rasterizer, the
// shadow alpha pass and the path tracers all index the same table.
//
// The OpenPBR half is the block shared with the rasteriser; the renderer extensions and per-lobe parameters
// follow it.
struct PbrMaterialParameters {
    OpenPbrSurfaceParams surface;

    vec2 uvScale;
    float normalScale;
    float aoStrength;

    uint samplerIndex;
    uint baseColorTex;
    uint normalTex;
    uint ormTex;

    uint emissionTex;
    float geometryOpacity;
    float alphaCutoff;
    uint flags;

    // Per-lobe parameters, selected by `type`; exactly one lobe's fields are live at a time. The rasteriser
    // reads nothing past `flags` but must still declare them: the struct's size is the array stride.
    vec3 complexIorEta;
    int microfacetType;

    vec3 complexIorK;
    int type;
};

#endif // CRISP_PBR_MATERIAL_GLSL
