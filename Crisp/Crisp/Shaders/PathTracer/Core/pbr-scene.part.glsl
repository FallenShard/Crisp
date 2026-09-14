#ifndef CRISP_PATH_TRACER_PBR_SCENE_GLSL
#define CRISP_PATH_TRACER_PBR_SCENE_GLSL

#include "../../Common/openpbr-surface.part.glsl"
#include "instance.part.glsl"

// The OpenPBR half is one nested block shared with the rasterizer, the other tracer and BsdfParameters;
// everything after it is a Crisp renderer extension. Must match PbrMaterialParams in Materials/PbrMaterial.hpp.
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
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PbrMaterials {
    PbrMaterialParameters data[];
};

#ifndef CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer EnvironmentCdf { float data[]; };
#endif

// Must match PathTracedViewAddresses in Scenes/PathTracedView.hpp.
layout(push_constant, scalar) uniform PathTracedViewAddresses {
    PathTracedInstances instances;
    PbrMaterials materials;
    EnvironmentCdf environmentCdf;
} scene;

#endif // CRISP_PATH_TRACER_PBR_SCENE_GLSL
