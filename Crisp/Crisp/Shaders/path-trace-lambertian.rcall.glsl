#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"
#include "Parts/math-constants.part.glsl"
#include "Parts/warp.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

layout(set = 1, binding = 3) buffer BrdfParams
{
    BrdfParameters brdfParams[];
};

void main()
{
    bsdf.sampleDirection = squareToCosineHemisphere(bsdf.unitSample);
    bsdf.samplePdf = InvTwoPI;
    bsdf.eval = brdfParams[bsdf.materialId].albedo;
}
