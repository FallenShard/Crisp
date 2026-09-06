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

// environment-distribution.part.glsl uses the analytic tracer's azimuth, u = atan2(x, -z) / 2pi, which is the
// mirror of what equirect-to-cube.frag.glsl hands the skybox and what cmgen bakes into the prefiltered maps:
// u_tracer = 1 - u_raster for every direction. Reflecting X maps between the two. It is an isometry, so
// solid-angle densities carry across untouched and only the lookup moves.
//
// The reflection lives here rather than in the shared header because the analytic tracer is validated against
// Mitsuba in that convention; changing it there would break those references.
vec3 toEnvironmentDistributionSpace(const vec3 direction) {
    return vec3(-direction.x, direction.y, direction.z);
}

vec3 evaluateEnvironmentRadiance(const vec3 direction) {
    const vec2 uv = environmentDirectionToUv(toEnvironmentDistributionSpace(direction));
    return textureLod(CRISP_ENVIRONMENT_EQUIRECT, uv, 0.0f).rgb;
}

vec3 sampleEnvironmentWorldDirection(
    const EnvironmentCdf distribution,
    const uint width,
    const uint height,
    const vec2 sampleValue,
    out float pdf) {
    vec2 uv;
    const vec3 direction = sampleEnvironmentDirection(distribution, width, height, sampleValue, uv, pdf);
    return toEnvironmentDistributionSpace(direction);
}

float environmentWorldDirectionPdf(
    const EnvironmentCdf distribution, const uint width, const uint height, const vec3 direction) {
    return environmentDirectionPdf(distribution, width, height, toEnvironmentDistributionSpace(direction));
}

// Balance heuristic. Power 1 rather than 2: it is the provably lowest-variance single-sample combination, and
// the squared form only pays off when one strategy is far better than the other everywhere.
float environmentMisWeight(const float pdfA, const float pdfB) {
    const float total = pdfA + pdfB;
    return total > 0.0f ? pdfA / total : 0.0f;
}

#endif // CRISP_PATH_TRACER_PBR_ENVIRONMENT_GLSL
