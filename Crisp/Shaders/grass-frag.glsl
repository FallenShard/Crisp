#version 460 core

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 eyePos;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform LightTransforms
{
    mat4 LVP[4];
};
layout(set = 1, binding = 0) uniform sampler2D shadowMap[4];

const vec4 lightPos = vec4(5.0f, 5.0f, 5.0f, 1.0f);
const vec4 lightDir = vec4(normalize(vec3(1.0f, 1.0f, 1.0f)), 0.0f);

const vec3 ndcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 ndcMax = vec3(+1.0f, +1.0f, 1.0f);

const vec4 splitNear = vec4(0.1f,  10.0f, 20.0f, 40.0f);
const vec4 splitFar  = vec4(10.0f, 20.0f, 40.0f, 80.0f);

float getCsmShadowCoeff()
{
    vec4 lsPos0 = LVP[0] * vec4(worldPos, 1.0f);
    vec4 lsPos1 = LVP[1] * vec4(worldPos, 1.0f);
    vec4 lsPos2 = LVP[2] * vec4(worldPos, 1.0f);
    vec4 lsPos3 = LVP[3] * vec4(worldPos, 1.0f);

    vec4 eyeDist = abs(eyePos.zzzz);
    vec4 weight = vec4(greaterThan(eyeDist, splitNear)) * vec4(lessThan(eyeDist, splitFar));

    lsPos0 = lsPos0 * weight.x + lsPos1 * weight.y + lsPos2 * weight.z + lsPos3 * weight.w;
    vec3 ndcPos = lsPos0.xyz / lsPos0.w;

    int idx = 3;
    if (weight.z == 1) idx = 2;
    else if (weight.y == 1) idx = 1;
    else if (weight.x == 1) idx = 0;

    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
        return 0.1f;

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;
    float shadowMapDepth = texture(shadowMap[idx], texCoord).r + 0.0001f;

    ivec2 shadowMapSize = textureSize(shadowMap[0], 0);
    vec2 texelSize = vec2(1.0f) / shadowMapSize;

    return shadowMapDepth < ndcPos.z ? 0.1f : 1.0f;
}

void main()
{
    float shadow = getCsmShadowCoeff();
    vec3 color = vec3(0.0f, 0.5f, 0.0f);
    finalColor = vec4(color * shadow, 1.0f);
}