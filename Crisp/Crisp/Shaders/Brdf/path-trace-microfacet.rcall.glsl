#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../PathTracer/Core/types.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../Common/warp.part.glsl"
#include "../PathTracer/Core/scene.part.glsl"
#include "microfacet.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    const BrdfParameters material = scene.materials.data[brdf.materialId];
    const float ks = material.ks;
    const float alpha = material.microfacetAlpha;

    if (brdf.operation == kBrdfOperationSample) {
        vec2 unitSample = brdf.unitSample;

        if (unitSample.x < ks) {
            unitSample.x /= ks; // Reuse the sample here.
            const vec3 microfacetNormal = sampleGGXNormal(unitSample, alpha);
            brdf.wo = 2.0f * dot(microfacetNormal, brdf.wi) * microfacetNormal - brdf.wi; // Reflect.
            brdf.lobeType = kLobeTypeGlossy;
        } else {
            unitSample.x = (unitSample.x - ks) / (1.0 - ks); // Reuse the sample here.
            brdf.wo = squareToCosineHemisphere(unitSample);
            brdf.lobeType = kLobeTypeDiffuse;
        }
    } else {
        brdf.lobeType = kLobeTypeGlossy | kLobeTypeDiffuse;
    }

    brdf.f = evaluateMicrofacet(
        material.kd, material.ks, material.extIor, material.intIor, material.microfacetAlpha, brdf.wi, brdf.wo);
    brdf.pdf = microfacetPdf(brdf.wi, brdf.wo, material.ks, material.microfacetAlpha);
}
