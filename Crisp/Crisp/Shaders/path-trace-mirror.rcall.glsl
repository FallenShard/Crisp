#version 460 core
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/path-trace-payload.part.glsl"

layout(location = 0) callableDataInEXT BrdfSample brdf;

void main()
{
    brdf.wo = reflect(-brdf.wi, brdf.normal);
    brdf.pdf = 1.0f;
    brdf.f = vec3(1.0f);
    brdf.lobeType = kLobeTypeDelta;
}
