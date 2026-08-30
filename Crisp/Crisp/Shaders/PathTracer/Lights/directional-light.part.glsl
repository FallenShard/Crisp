#ifndef CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL
#define CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL

// `direction` is a normalized direction of light travel, matching the CPU scene contract.
vec3 sampleDirectionalLight(
    const vec3 direction,
    const vec3 irradiance,
    out vec3 shadowRayDir,
    out float shadowRayLen,
    out float lightPdf) {
    shadowRayDir = -direction;
    shadowRayLen = 1e30f;
    lightPdf = 1.0f; // Unit discrete mass; the continuous directional PDF is zero.
    return irradiance;
}

#endif // CRISP_PATH_TRACER_DIRECTIONAL_LIGHT_GLSL
