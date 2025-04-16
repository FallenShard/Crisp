#version 450 core

#extension GL_GOOGLE_include_directive: require

#include "Parts/view.part.glsl"

layout(vertices = 4) out;

layout(set = 0, binding = 0) uniform Transforms
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 2) uniform View {
    ViewParameters view;
};

layout(push_constant) uniform PushConstants
{
    layout(offset = 0) float worldScale;
    layout(offset = 4) float triangleSize;
};

float calculateTess(vec4 p1, vec4 p2, float diameter)
{
    vec4 center = 0.5f * (p1 + p2);
    vec4 eyeCenter = MV * center;
    vec4 eyeSurfPt = eyeCenter;
    eyeSurfPt.x += worldScale * diameter;

    vec4 clipCenter = view.P * eyeCenter;
    vec4 clipSurfPt = view.P * eyeSurfPt;

    clipCenter /= clipCenter.w;
    clipSurfPt /= clipSurfPt.w;

    clipCenter *= 0.5f;
    clipSurfPt *= 0.5f;

    clipCenter += 0.5f;
    clipSurfPt += 0.5f;

    clipCenter.xy *= view.screenSize;
    clipSurfPt.xy *= view.screenSize;

    float d = distance(clipCenter, clipSurfPt);

    return clamp(d / triangleSize, 1.0f, 32.0f);
}

void main()
{
    float s1 = abs(gl_in[0].gl_Position.x - gl_in[3].gl_Position.x);
    float s2 = abs(gl_in[0].gl_Position.x - gl_in[1].gl_Position.x);
    float sideLen = max(s1, s2);
    float diagLen = sqrt(2 * sideLen * sideLen);

    gl_TessLevelOuter[0] = calculateTess(gl_in[3].gl_Position, gl_in[0].gl_Position, sideLen);
    gl_TessLevelOuter[1] = calculateTess(gl_in[0].gl_Position, gl_in[1].gl_Position, sideLen);
    gl_TessLevelOuter[2] = calculateTess(gl_in[1].gl_Position, gl_in[2].gl_Position, sideLen);
    gl_TessLevelOuter[3] = calculateTess(gl_in[2].gl_Position, gl_in[3].gl_Position, sideLen);

    gl_TessLevelInner[0] = 0.5f * (gl_TessLevelOuter[0] + gl_TessLevelOuter[2]);
    gl_TessLevelInner[1] = 0.5f * (gl_TessLevelOuter[1] + gl_TessLevelOuter[3]);

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}