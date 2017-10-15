#version 450 core
 
layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 eyeNormal;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 1) uniform LightTransforms
{
    mat4 LVP[4];
};

layout(set = 0, binding = 2) uniform CameraParams
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

layout(set = 1, binding = 0) uniform sampler2D shadowMaps[4];

#define PI         3.14159265358979323846
#define InvPI      0.31830988618379067154
#define InvTwoPI   0.15915494309189533577
#define InvFourPI  0.07957747154594766788
#define SqrtTwo    1.41421356237309504880
#define InvSqrtTwo 0.70710678118654752440
const vec4 lightPos = vec4(5.0f, 5.0f, 5.0f, 1.0f);
//const vec4 lightDir = vec4(normalize(vec3(0.0f, 0.0f, 5.0f)), 0.0f);
const vec4 lightDir = vec4(normalize(vec3(1.0f, 1.0f, 1.0f)), 0.0f);

const vec3 ndcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 ndcMax = vec3(+1.0f, +1.0f, 1.0f);

const vec4 splitNear = vec4(0.1f, 10.0f, 30.0f, 70.0f);
const vec4 splitFar = vec4(10.0f, 30.0f, 70.0f, 150.0f);

vec3 shadowEstimate(vec3 worldPos, float bias, in sampler2D sMap, in mat4 lvp)
{
    vec4 lsPos = lvp * vec4(worldPos, 1.0f);
    vec4 lsPos1 = LVP[1] * vec4(worldPos, 1.0f);
    vec4 lsPos2 = LVP[2] * vec4(worldPos, 1.0f);
    vec4 lsPos3 = LVP[3] * vec4(worldPos, 1.0f);

    // eyeDist = abs(eyePos.zzzz);
    // weight = vec4(eyeDist > splitNear) * vec4(eyeDist < splitFar);

    // lsPos = lsPos0 * weight.x + lsPos1 * weight.y + lsPos2 * weight.z + lsPos3 * weight.w;


    vec3 projPos = lsPos.xyz / lsPos.w;

    if (any(lessThan(projPos, ndcMin)) || any(greaterThan(projPos, ndcMax)))
        return vec3(1.0f);

    //if (projPos.x < -1.0f || projPos.x > 1.0f || projPos.y < -1.0f || projPos.y > 1.0f)
    //	return vec3(1.0f);//return vec3(10000.0f, 0.0f, 0.0f);
    //
    //if (projPos.z > 1.0f)
    //	return vec3(0.0f, 10000.0f, 0.0f);
    //
    //if (projPos.z < 0.0f)
    //	return vec3(10000.0f, 10000.0f, 0.0f);
    //

    ivec2 texSize = textureSize(sMap, 0);
    vec2 texelSize = vec2(1.0f) / texSize;
    int radius = 3;
    float taps = (2 * radius + 1) * (2 * radius + 1);
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++)
    {
        for (int j = -radius; j <= radius; j++)
        {
            vec2 texCoord = projPos.xy * 0.5f + 0.5f + vec2(i, j) * texelSize;
            float shadowDepth = texture(sMap, texCoord).r;
            if (projPos.z > shadowDepth + bias)
                sum += 1.0f;
        }
    }

    //float shadowDepth = texture(sMap, projPos.xy * 0.5f + 0.5f).r;
    //if (projPos.z > shadowDepth + bias)
    //    return vec3(0.2f);

    return vec3(1.0f - 0.8f * sum / taps);
}

vec3 shadowDebug(vec3 worldPos)
{
    vec4 lsPos = LVP[0] * vec4(worldPos, 1.0f);
    vec3 projPos = lsPos.xyz;// / lsPos.w;

    if (projPos.x < -1.0f || projPos.x > 1.0f || projPos.y < -1.0f || projPos.y > 1.0f)
        return vec3(10000.0f, 0.0f, 0.0f);

    if (projPos.z > 1.0f)
        return vec3(10000.0f, 0.0f, 10000.0f);

    if (projPos.z < 0.0f)
        return vec3(10000.0f, 10000.0f, 0.0f);

    return vec3(0.0f, 10000.0f, 0.0f);
}
 
vec3 getColor(vec3 pos)
{
    float dist = abs(pos.z);
    if (dist > 20.0f)
        return vec3(0.75, 0.15, 0.15) * InvPI;
    else if (dist > 10.0f)
        return vec3(0.75, 0.75, 0.15) * InvPI;
    else if (dist > 5.0f)
        return vec3(0.15, 0.75, 0.15) * InvPI;
    else
        return vec3(0.15, 0.15, 0.75) * InvPI;
}

void main()
{
    //vec3 eyeLightPos = (V * lightPos).xyz;
    //
    //vec3 wi = eyeLightPos - eyePos;
    //
    //float dist2 = dot(wi, wi);
    //float dist = sqrt(dist2);
    //wi = wi / dist;
    //
    //vec3 lightPower = vec3(500.0f);
    //
    //float cosThetaI = max(0, dot(wi, eyeNormal));
    //
    //vec3 f = vec3(0.12f, 0.45f, 0.67f) * InvPI;
    //vec3 Li = lightPower * InvFourPI / dist2;
    //
    //float bias   = 0.0005f * tan(acos(cosThetaI));
    //float V = shadowEstimate(worldPos, bias);
    //
    //finalColor = vec4(V * f * Li * cosThetaI, 1.0f);

    vec3 wi = (V * lightDir).xyz;
    vec3 intensity = vec3(1.0f);
    
    float cosThetaI = max(0, dot(wi, eyeNormal));
    
    vec3 f = getColor(eyePos);
    vec3 Li = intensity;
    
    float bias       = 0.005f;//0.0005f * tan(acos(cosThetaI));

    float eyeDist = abs(eyePos.z);
    int sam = 0;
    if (eyeDist > 20.0f)
    {
        sam = 3;
    }
    else if (eyeDist > 10.0f)
    {
        sam = 2;
    }
    else if (eyeDist > 5.0f)
    {
        sam = 1;
    }

    vec3 V = shadowEstimate(worldPos, bias, shadowMaps[sam], LVP[sam]);

    
    
    finalColor = vec4(V * f * Li * cosThetaI, 1.0f);

    //finalColor = vec4(shadowDebug(worldPos), 1.0f);
    //finalColor = vec4(worldPos / 5.0f, 1.0f);
}