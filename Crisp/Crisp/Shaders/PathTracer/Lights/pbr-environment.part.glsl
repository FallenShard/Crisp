#ifndef CRISP_PATH_TRACER_PBR_ENVIRONMENT_GLSL
#define CRISP_PATH_TRACER_PBR_ENVIRONMENT_GLSL

// The environment lookup for the PBR path-traced view. Both the miss shader and next-event estimation go
// through this, because MIS weights are only valid when the two strategies integrate the same function.
//
// The including shader supplies CRISP_ENVIRONMENT_EQUIRECT as a sampler2D over the equirectangular map.

#ifndef CRISP_ENVIRONMENT_EQUIRECT
#error "Define CRISP_ENVIRONMENT_EQUIRECT as a sampler2D before including pbr-environment.part.glsl."
#endif

#include "environment-distribution.part.glsl"

vec3 evaluateEnvironmentRadiance(const vec3 direction) {
    const vec2 uv = environmentDirectionToUv(direction);
    return textureLod(CRISP_ENVIRONMENT_EQUIRECT, uv, 0.0f).rgb;
}

// Wraps sampleEnvironmentDirection to drop the uv the caller does not need.
vec3 sampleEnvironmentLightDirection(
    const EnvironmentCdf distribution,
    const uint width,
    const uint height,
    const vec2 sampleValue,
    out float pdf) {
    vec2 uv;
    return sampleEnvironmentDirection(distribution, width, height, sampleValue, uv, pdf);
}

// Balance heuristic. Power 1 rather than 2: it is the provably lowest-variance single-sample combination, and
// the squared form only pays off when one strategy is far better than the other everywhere.
float environmentMisWeight(const float pdfA, const float pdfB) {
    const float total = pdfA + pdfB;
    return total > 0.0f ? pdfA / total : 0.0f;
}

#endif // CRISP_PATH_TRACER_PBR_ENVIRONMENT_GLSL
