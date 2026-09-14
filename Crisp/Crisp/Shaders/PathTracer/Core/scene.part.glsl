#ifndef CRISP_PATH_TRACER_SCENE_GLSL
#define CRISP_PATH_TRACER_SCENE_GLSL

#include "../../Common/pbr-material.part.glsl"
#include "instance.part.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceMaterials {
    PbrMaterialParameters data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceLights {
    LightParameters data[];
};

#ifndef CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer EnvironmentCdf { float data[]; };
#endif

// Must match RayTracingSceneAddresses in Scenes/RayTracingSceneData.hpp.
layout(push_constant, scalar) uniform RayTracingSceneAddresses {
    PathTracedInstances instances;
    PathTraceMaterials materials;
    PathTraceLights lights;
    EnvironmentCdf environmentCdf;
}
scene;

#endif // CRISP_PATH_TRACER_SCENE_GLSL
