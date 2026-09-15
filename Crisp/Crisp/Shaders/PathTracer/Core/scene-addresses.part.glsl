#ifndef CRISP_PATH_TRACER_SCENE_ADDRESSES_GLSL
#define CRISP_PATH_TRACER_SCENE_ADDRESSES_GLSL

#include "../../Common/pbr-material.part.glsl"
#include "../Lights/light-types.part.glsl"
#include "../Lights/environment-distribution.part.glsl"
#include "instance.part.glsl"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PathTracedMaterials {
    PbrMaterialParameters data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTracedLights {
    LightParameters data[];
};

// Must match PathTracedSceneAddresses in Scenes/PathTracer.hpp.
//
// energyCompensation rides here rather than in the integrator block because it is the only per-view setting the
// closest-hit stage needs: taking it from the push constant is what lets one hit shader serve tracers whose
// integrator blocks still differ.
layout(push_constant, scalar) uniform PathTracedSceneAddresses {
    PathTracedInstances instances;
    PathTracedMaterials materials;
    PathTracedLights lights; // Null when the view has no analytic lights.
    EnvironmentCdf environmentCdf;
    uint energyCompensation;
    uint pad0;
}
scene;

#endif // CRISP_PATH_TRACER_SCENE_ADDRESSES_GLSL
