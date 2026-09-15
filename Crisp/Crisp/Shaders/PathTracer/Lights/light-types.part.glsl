#ifndef CRISP_PATH_TRACER_LIGHT_TYPES_GLSL
#define CRISP_PATH_TRACER_LIGHT_TYPES_GLSL

const int kLightArea = 0;
const int kLightPoint = 1;
const int kLightDirectional = 2;

// Must match LightParameters in Scenes/RayTracingSceneParser.hpp. Kept apart from types.part.glsl so a stage
// can take the light table without the analytic tracer's sampler dimension budget.
struct LightParameters {
    int type;
    int meshId;
    int pad0;
    int pad1;
    vec3 emission; // Area radiance, point power, or directional irradiance.
    float pad2;
    vec3 positionOrDirection;
    float pad3;
};

#endif // CRISP_PATH_TRACER_LIGHT_TYPES_GLSL
