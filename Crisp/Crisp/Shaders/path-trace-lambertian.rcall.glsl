#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"
#include "Common/math-constants.part.glsl"
#include "Common/warp.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

layout(set = 1, binding = 3) buffer BrdfParams
{
    BrdfParameters brdfParams[];
};

void main()
{
    brdf.wo = squareToCosineHemisphere(brdf.unitSample);
    brdf.pdf = squareToCosineHemispherePdf(brdf.wo);
    brdf.f = brdfParams[brdf.materialId].albedo;
    brdf.lobeType = kLobeTypeDiffuse;
}
