#ifndef CRISP_PATH_TRACER_PBR_SCENE_GLSL
#define CRISP_PATH_TRACER_PBR_SCENE_GLSL

#include "../../Common/pbr-material.part.glsl"
#include "instance.part.glsl"

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
