#ifndef CRISP_PATH_TRACER_ENVIRONMENT_LIGHT_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_LIGHT_GLSL

#include "environment-distribution.part.glsl"
#include "light-types.part.glsl"

// The including shader supplies environmentMap and environmentSampler.

vec3 evaluateEnvironmentLight(const vec3 direction) {
    if (integrator.environmentEnabled == 0) {
        return vec3(0.0f);
    }
    const vec2 uv = environmentDirectionToUv(direction);
    return integrator.environmentIntensity *
        textureLod(sampler2D(environmentMap, environmentSampler), uv, 0.0f).rgb;
}

float computeEnvironmentLightPdf(const vec3 direction) {
    if (integrator.environmentEnabled == 0) {
        return 0.0f;
    }
    return environmentDirectionPdf(
        scene.environmentCdf,
        uint(integrator.environmentWidth),
        uint(integrator.environmentHeight),
        direction);
}

LightSample sampleEnvironmentLight(inout Sampler rng) {
    LightSample ls;
    ls.isDelta = false;
    ls.distance = 1e30f;

    vec2 uv;
    ls.direction = sampleEnvironmentDirection(
        scene.environmentCdf,
        uint(integrator.environmentWidth),
        uint(integrator.environmentHeight),
        next2D(rng),
        uv,
        ls.pdf);
    ls.weight = ls.pdf > 0.0f ? evaluateEnvironmentLight(ls.direction) / ls.pdf : vec3(0.0f);
    return ls;
}

#endif // CRISP_PATH_TRACER_ENVIRONMENT_LIGHT_GLSL
