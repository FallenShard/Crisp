#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdfSample;

void main()
{
    bsdfSample.sampleDirection = reflect(-bsdfSample.wi, bsdfSample.normal);
    bsdfSample.samplePdf = 0.0f;
    bsdfSample.eval = vec3(1.0f);
}
