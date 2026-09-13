#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../PathTracer/Core/types.part.glsl"
#include "../Common/math-constants.part.glsl"
#include "../PathTracer/Core/scene.part.glsl"
#include "dielectric.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main() {
    brdf.lobeType = kLobeTypeDelta;
    if (brdf.operation == kBrdfOperationEvaluate) {
        brdf.wo = vec3(0.0f);
        brdf.pdf = 0.0f;
        brdf.f = vec3(0.0f);
        return;
    }

    const float intIOR = scene.materials.data[brdf.materialId].surface.specularIor;
    const float extIOR = scene.materials.data[brdf.materialId].extIor;
    const float etaRatio = intIOR / extIOR;
    const float cosThetaI = dot(brdf.normal, brdf.wi);
    const vec3 localNormal = cosThetaI < 0.0f ? -brdf.normal : brdf.normal;
    const float eta = cosThetaI < 0.0f ? etaRatio : 1.0f / etaRatio;
    const float cosine = cosThetaI < 0.0f ? etaRatio * cosThetaI : -cosThetaI;
    float cosThetaT = 0.0f;
    const float fresnel = fresnelDielectric(cosThetaI, extIOR, intIOR, cosThetaT);

    if (brdf.unitSample[0] <= fresnel) {
        brdf.wo = reflect(-brdf.wi, localNormal);
        brdf.pdf = fresnel;
        brdf.f = vec3(fresnel);
    } else {
        brdf.wo = refract(-brdf.wi, localNormal, eta);
        brdf.pdf = 1.0f - fresnel;
        brdf.f = vec3(brdf.pdf * eta * eta);
    }
}
