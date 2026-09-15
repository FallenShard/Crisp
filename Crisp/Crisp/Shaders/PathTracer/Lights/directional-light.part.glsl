#ifndef CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL
#define CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL

#include "light-types.part.glsl"

// `direction` is a normalized direction of light travel, matching the CPU scene contract.
LightSample sampleDirectionalLight(const vec3 direction, const vec3 irradiance) {
    LightSample ls;
    ls.isDelta = true;
    ls.direction = -direction;
    ls.distance = 1e30f;
    ls.pdf = 1.0f; // Unit discrete mass; the continuous directional PDF is zero.
    ls.weight = irradiance;
    return ls;
}

vec3 evaluateDirectionalLight() {
    return vec3(0.0f);
}

float computeDirectionalLightPdf() {
    return 0.0f;
}

#endif // CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL
