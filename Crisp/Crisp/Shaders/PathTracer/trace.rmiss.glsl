#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Core/hit-info.part.glsl"

layout(location = 0) rayPayloadInEXT HitInfo hitInfo;

void main() {
    hitInfo.tHit = -1.0f;
    hitInfo.lightId = -1;
}
