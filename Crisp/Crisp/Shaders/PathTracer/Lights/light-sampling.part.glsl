#ifndef CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL
#define CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL

#include "area-light.part.glsl"
#include "directional-light.part.glsl"
#include "environment-light.part.glsl"
#include "point-light.part.glsl"

// Dispatch over the scene's light table, the counterpart to sampleBsdf and evaluateBsdf in BSDFs/. Each light
// type answers sample, evaluate and pdf for itself; this layer only chooses which one to ask and folds in the
// probability of having picked it.
//
// The environment occupies the slot just past the finite lights, so one index space covers every light and the
// sampler needs no special case for it.
uint environmentLightIndex() {
    return uint(integrator.lightCount - integrator.environmentEnabled);
}

float lightSelectionPdf() {
    return 1.0f / float(integrator.lightCount);
}

LightSample sampleLight(inout Sampler rng, const uint lightId, const vec3 refPoint) {
    if (lightId >= environmentLightIndex()) {
        return sampleEnvironmentLight(rng);
    }

    const LightParameters light = scene.lights.data[lightId];
    if (light.type == kLightPoint) {
        return samplePointLight(light.positionOrDirection, light.emission, refPoint);
    }
    if (light.type == kLightDirectional) {
        return sampleDirectionalLight(light.positionOrDirection, light.emission);
    }
    return sampleAreaLight(rng, uint(light.meshId), light.emission, refPoint);
}

// `direction` is the unit direction from the shading point toward the light; `toLight` and `lightNormal`
// describe the point a ray landed on and are ignored by a light with no geometry.
LightEval evaluateLight(
    const uint lightId, const vec3 direction, const vec3 toLight, const vec3 lightNormal) {
    LightEval le;
    const float selectionPdf = lightSelectionPdf();

    if (lightId >= environmentLightIndex()) {
        le.radiance = evaluateEnvironmentLight(direction);
        le.pdf = selectionPdf * computeEnvironmentLightPdf(direction);
        return le;
    }

    const LightParameters light = scene.lights.data[lightId];
    if (light.type == kLightPoint) {
        le.radiance = evaluatePointLight();
        le.pdf = selectionPdf * computePointLightPdf();
        return le;
    }
    if (light.type == kLightDirectional) {
        le.radiance = evaluateDirectionalLight();
        le.pdf = selectionPdf * computeDirectionalLightPdf();
        return le;
    }
    le.radiance = evaluateAreaLight(light.emission, lightNormal, -toLight);
    le.pdf = selectionPdf * computeAreaLightPdf(uint(light.meshId), toLight, lightNormal);
    return le;
}

float computeLightPdf(const uint lightId, const vec3 direction, const vec3 toLight, const vec3 lightNormal) {
    return evaluateLight(lightId, direction, toLight, lightNormal).pdf;
}

// Picks one light uniformly. The choice costs a dimension whether or not the scene has more than one light, so
// that a path's cursor lands in the same place either way.
LightSample sampleUniformLight(inout Sampler rng, const vec3 refPoint) {
    const uint lightId = nextRange(rng, integrator.lightCount);
    const float selectionPdf = lightSelectionPdf();

    LightSample ls = sampleLight(rng, lightId, refPoint);
    ls.pdf *= selectionPdf;
    ls.weight /= selectionPdf;
    return ls;
}

#endif // CRISP_PATH_TRACER_LIGHT_SAMPLING_GLSL
