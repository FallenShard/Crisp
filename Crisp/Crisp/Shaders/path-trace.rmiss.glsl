#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "Parts/path-trace-payload.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

void main()
{
    hitInfo.tHit = -1;
    hitInfo.lightId = -1;
}