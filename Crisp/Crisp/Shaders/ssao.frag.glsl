#version 450 core

#extension GL_GOOGLE_include_directive: require

#include "Common/view.part.glsl"

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform sampler2D normalDepthTex;
layout(set = 0, binding = 1) uniform View {
    ViewParameters view;
};
layout(set = 0, binding = 2) uniform SamplesBuffer
{
    vec4 samples[512];
};
layout(set = 0, binding = 3) uniform sampler2D noiseTex;

layout(push_constant) uniform AoParams
{
    layout(offset = 0) int numSamples;
    layout(offset = 4) float radius;
} aoParams;

float getEyeDepth(float fragDepth)
{
    //return P[3][2] / (1.0f - 2.0f * fragDepth - P[2][2]);
    return view.P[3][2] / (-fragDepth - view.P[2][2]);
}

vec3 screenToEye(vec2 uv, float eyeDepth)
{
    uv = 2.0f * uv - 1.0f;
    return vec3(-uv.x / view.P[0][0], -uv.y / view.P[1][1], 1.0f) * eyeDepth;
}

vec3 getEyePos(vec2 uv)
{
    float eyeDepth = getEyeDepth(texture(normalDepthTex, uv).w);
    return screenToEye(uv, eyeDepth);
}

vec4 projectToScreen(vec3 eyePos)
{
	vec4 clipPos = view.P * vec4(eyePos, 1.0f);
	float invW = 1.0f / clipPos.w;
	vec3 ndcPos = clipPos.xyz * invW;
    //vec3 screenPos = ndcPos * 0.5f + 0.5f;
    vec3 screenPos = vec3(ndcPos.xy * 0.5f + 0.5f, ndcPos.z);
	return vec4(screenPos, invW);
}

void main()
{
    vec4 normalDepth = texture(normalDepthTex, texCoord);

    vec3 origin = screenToEye(texCoord, getEyeDepth(normalDepth.w));
    vec3 normal = normalize(normalDepth.rgb);
    origin += 0.01 * normal;

    vec2 noiseScale = view.screenSize / vec2(4.0f, 4.0f);
    vec3 rvec = texture(noiseTex, texCoord * noiseScale).xyz * 2.0f - 1.0f;
    vec3 tangent = normalize(rvec - normal * dot(rvec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (int i = 0; i < aoParams.numSamples; i++)
    {
        vec3 samplePos = tbn * samples[i].xyz;
        samplePos = origin + samplePos * aoParams.radius;
        vec4 sampleProj = projectToScreen(samplePos);
        float sampleEyeDepth = getEyeDepth(texture(normalDepthTex, sampleProj.xy).w);
        float rangeCheck = abs(origin.z - sampleEyeDepth) < aoParams.radius ? 1.0f : 0.0f;
        occlusion += (sampleEyeDepth >= samplePos.z ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - occlusion / aoParams.numSamples;

    finalColor = vec4(vec3(occlusion), 1.0f);
}
