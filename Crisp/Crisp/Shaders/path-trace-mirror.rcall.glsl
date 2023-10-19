#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"

layout(location = 0) callableDataInEXT MirrorBsdfSample bsdf;

void main()
{
    bsdf.sampleDirection = reflect(-bsdf.wi, bsdf.normal);
    bsdf.samplePdf = 0.0f;
    bsdf.eval = vec3(1.0f);
}
