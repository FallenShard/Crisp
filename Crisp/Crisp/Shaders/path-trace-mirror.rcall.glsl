#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-tracer-payload.part.glsl"

layout(location = 0) callableDataInEXT BsdfSample bsdf;

void main()
{
    bsdf.wo = reflect(-bsdf.wi, bsdf.normal);
    bsdf.pdf = 0.0f;
    bsdf.f = vec3(1.0f);
}
