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

struct LightEval {
    vec3 radiance; // Radiance arriving at the shading point along the queried direction.
    float pdf;     // Solid-angle density with which light sampling would have chosen that direction.
};

struct LightSample {
    vec3 direction; // Out, unit, from the shading point toward the light.
    float distance; // Out, shadow ray length.
    vec3 weight;    // Out, radiance / pdf.
    float pdf;      // Out; a delta light reports unit discrete mass rather than a density.
    bool isDelta;   // Out.
};

#endif // CRISP_PATH_TRACER_LIGHT_TYPES_GLSL
