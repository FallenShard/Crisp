#version 460 core

layout(location = 0) in vec3 inEyePos;
layout(location = 1) in vec3 inEyeNormal;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform Material
{
    vec4 surface;
    vec4 highlight;
};

struct LightDescriptor
{
    mat4 V;
    mat4 P;
    mat4 VP;
    vec4 position;
    vec4 direction;
    vec4 spectrum;
    vec4 params;
};

layout(set = 1, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
} camera;

const vec3 NdcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 NdcMax = vec3(+1.0f, +1.0f, 1.0f);

// Directional Light
layout(set = 1, binding = 2) uniform CascadedLight
{
    LightDescriptor cascadedLight[4];
};

layout(set = 1, binding = 3) uniform sampler2DArray cascadedShadowMapArray;

// ----- Cascaded Shadow Mapping
bool isInCascade(vec3 worldPos, mat4 lightVP)
{
    vec4 lightSpacePos = lightVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    return all(greaterThan(ndcPos, NdcMin)) && all(lessThan(ndcPos, NdcMax));
}

// Check-in-bounds based
float getCsmShadowCoeffDebug(vec3 worldPos, out vec3 color, float bias)
{
    color = vec3(0.0f, 0.0f, 1.0f);
    int cascadeIndex = 3;
    if (isInCascade(worldPos, cascadedLight[0].VP))
    {
       color = vec3(1.0f, 0.0f, 0.0f);
       cascadeIndex = 0;
    }
    else if (isInCascade(worldPos, cascadedLight[1].VP))
    {
       color = vec3(1.0f, 1.0f, 0.0f);
       cascadeIndex = 1;
    }
    else if (isInCascade(worldPos, cascadedLight[2].VP))
    {
       color = vec3(0.0f, 1.0f, 0.0f);
       cascadeIndex = 2;
    }

    vec4 lightSpacePos = cascadedLight[cascadeIndex].VP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, NdcMin)) || any(greaterThan(ndcPos, NdcMax)))
        return 1.0f;

    vec3 texCoord = vec3(ndcPos.xy * 0.5f + 0.5f, cascadeIndex);

    ivec2 size = textureSize(cascadedShadowMapArray, 0).xy;
    vec2 texelSize = vec2(1) / size;

    //float shadowMapDepth = texture(cascadedShadowMapArray, texCoord).r;
    //return shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;

    const int pcfRadius = 3;
    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    float amount  = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord.xy + vec2(i, j) * texelSize;
            float shadowMapDepth = texture(cascadedShadowMapArray, vec3(tc, cascadeIndex)).r;
            amount += shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;
        }
    }

    return amount / numSamples;
}

void main()
{
    const vec3 eyeV = -normalize(inEyePos);
    const vec3 eyeN = normalize(inEyeNormal);
    const vec3 eyeL = vec4(camera.V * cascadedLight[0].direction).xyz;

    const float t = (dot(eyeN, eyeL) + 1.0f) * 0.5f;
    const vec3 cool = vec3(0.0f, 0.0f, 0.55f) + 0.25f * surface.rgb;
    const vec3 warm = vec3(0.3f, 0.3f, 0.0f)  + 0.25f * surface.rgb;

    const vec3 eyeR = reflect(-eyeL, eyeN);

    const float s = clamp(100 * dot(eyeR, eyeV) - 99, 0.0f, 1.0f); 
    

    // float bias = max(0.05 * (1.0 - dot(eyeN, eyeL)), 0.005);  
    vec3 csmColor;
    float csm = getCsmShadowCoeffDebug(inWorldPos, csmColor, 0.005f);
    vec3 mask2 = vec3(1.0f, 1.0f - csm, 1.0f);

    csm = csm * 0.5f + 0.5f;

    vec3 color = s * highlight.rgb + (1 - s) * mix(cool, warm, t);

    vec3 tileSize = vec3(5.0f);
    vec3 squarePos = mod(inWorldPos, tileSize);
    vec3 tileColor = vec3(0.85);
    if (any(lessThan(squarePos.xz, 0.05 * tileSize.xz)) || any(greaterThan(squarePos.xz, 0.95 * tileSize.xz)))
        tileColor = vec3(0.15);
        
    outColor = vec4(csm * tileColor, 1.0f);
    //outColor = vec4(csm, shadowCoeff);
}