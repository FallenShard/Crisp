#version 450 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 eyePosition;
layout(location = 3) in vec3 eyeTangent;
layout(location = 4) in vec3 eyeBitangent;
layout(location = 5) in vec3 worldPos;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

layout(set = 0, binding = 2) uniform sampler2D diffuseTex;
layout(set = 0, binding = 3) uniform sampler2D metallicTex;
layout(set = 0, binding = 4) uniform sampler2D roughnessTex;
layout(set = 0, binding = 5) uniform sampler2D normalTex;
layout(set = 0, binding = 6) uniform sampler2D aoTex;

layout(set = 1, binding = 0) uniform Light
{
    mat4 VP;
    mat4 V;
    mat4 P;
    vec4 position;
    vec3 spectrum;
} light;

layout(set = 1, binding = 1) uniform sampler2D shadowMap;

layout(set = 1, binding = 2) uniform CascadeSplit
{
    vec4 lo;
    vec4 hi;
} cascadeSplit;

layout(set = 1, binding = 3) uniform LightTransforms
{
    mat4 VP[4];
} lightTransforms;

layout (set = 2, binding = 0) uniform samplerCube irrMap;
layout (set = 2, binding = 1) uniform samplerCube refMap;
layout (set = 2, binding = 2) uniform sampler2D brdfLut;

const vec3 ndcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 ndcMax = vec3(+1.0f, +1.0f, 1.0f);

bool isInCascade(int cascadeIndex)
{
    vec4 lightSpacePos = lightTransforms.VP[cascadeIndex] * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;
    
    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
       return false;
    else
       return true;
}

float getShadowCoeff(float bias, mat4 lightvp, vec2 atlasOffset)
{
    vec4 lightSpacePos = lightvp * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;
 
    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
        return 1.0f;
 
    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;
    texCoord = texCoord * 0.5f + atlasOffset;
 
    ivec2 size = textureSize(shadowMap, 0);
    vec2 texelSize = vec2(1) / size;
 
    int radius = 5;
    float amount  = 0.0f;
    float samples = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            tc = clamp(tc, atlasOffset + texelSize * 0.5, atlasOffset + vec2(0.5f, 0.5f) - texelSize * 0.5);
            //if (all(greaterThan(tc, atlasOffset)) && all(lessThan(tc, atlasOffset + vec2(0.5f))))
            //{
                float shadowMapDepth = texture(shadowMap, tc).r;
                amount += shadowMapDepth < ndcPos.z - bias ? 0.0f : 1.0f;
                samples += 1.0f;
            //}
            
            
        }
    }
 
    float numSamples = (2 * radius + 1) * (2 * radius + 1);
 
    return amount / samples;
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num    = a2;
    float denom  = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom        = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = r * r / 8.0f;

    float num   = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 getNormal(in vec2 uv)
{
    vec3 normal  = normalize(eyeNormal);
    vec3 tangent = normalize(eyeTangent);
    vec3 bitangent = normalize(eyeBitangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec3 n = texture(normalTex, uv).xyz;
    n = normalize(n * 2.0f - 1.0f);
    return normalize(TBN * n);
}

float calculateAttenuation(in vec3 lightVec)
{
    float dist = length(lightVec);
    return 1.0f / (dist * dist);
}

vec3 estimateRadiance(in vec3 lightVec)
{
    float attenuation = calculateAttenuation(lightVec);
    float distFactor = attenuation;//mix(1.0f, attenuation, light.position.w);
    return light.spectrum * distFactor;
}

float getCsmShadowCoeff()
{
    return 1.0f;
}

// Check-in-bounds based
float getCsmShadowCoeffDebug(float cosTheta, out vec3 color)
{
    color = vec3(0.0f, 0.0f, 1.0f);
    vec2 tcOffset = vec2(0.5f, 0.5f);
    mat4 lightVP = lightTransforms.VP[3];
    if (isInCascade(0))
    {
        color = vec3(1.0f, 0.0f, 0.0f);
        tcOffset = vec2(0.0f, 0.0f);
        lightVP = lightTransforms.VP[0];
    }
    else if (isInCascade(1))
    {
        color = vec3(1.0f, 1.0f, 0.0f);
        lightVP = lightTransforms.VP[1];
        tcOffset = vec2(0.5f, 0.0f);
    }
    else if (isInCascade(2))
    {
        color = vec3(0.0f, 1.0f, 0.0f);
        lightVP = lightTransforms.VP[2];
        tcOffset = vec2(0.0f, 0.5f);
    }

    color *= 0.3f;

    float bias = 0.005 * tan(acos(cosTheta)); // cosTheta is dot( n,l ), clamped between 0 and 1
    bias = clamp(bias, 0.0f, 0.01f);
    return getShadowCoeff(bias, lightVP, tcOffset);
}

// Depth based
float getCsmShadowCoeffDebugDepth(float zDist, float cosTheta, out vec3 color)
{
    color = vec3(1.0f, 0.0f, 0.0f);
    vec2 tcOffset = vec2(0.0f, 0.0f);
    mat4 lightVP = lightTransforms.VP[0];
    if (zDist > cascadeSplit.lo[3])
    {
        color = vec3(0.0f, 0.0f, 1.0f);
        tcOffset = vec2(0.5f, 0.5f);
        lightVP = lightTransforms.VP[3];
    }
    else if (zDist > cascadeSplit.lo[2])
    {
        color = vec3(0.0f, 1.0f, 0.0f);
        lightVP = lightTransforms.VP[2];
        tcOffset = vec2(0.0f, 0.5f);
    }
    else if (zDist > cascadeSplit.lo[1])
    {
        color = vec3(1.0f, 1.0f, 0.0f);
        lightVP = lightTransforms.VP[1];
        tcOffset = vec2(0.5f, 0.0f);
    }

    color *= 0.3f;

    float bias = 0.005 * tan(acos(cosTheta)); // cosTheta is dot( n,l ), clamped between 0 and 1
    bias = clamp(bias, 0.0f, 0.01f);
    return getShadowCoeff(bias, lightVP, tcOffset);
}

void main()
{
    const vec3  albedo    = texture(diffuseTex, texCoord).rgb;
    const float roughness = texture(roughnessTex, texCoord).r;
    const float metallic  = texture(metallicTex, texCoord).r;
    const float ao        = texture(aoTex, texCoord).r;

    const vec3 eyeN = getNormal(texCoord);
    const vec3 eyeV = normalize(-eyePosition);
    const vec3 eyeR = reflect(-eyeV, eyeN);

    const vec3 F0 = mix(vec3(0.04), albedo, metallic);

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);

    const vec3 worldN = (inverse(V) * vec4(eyeN, 0.0f)).rgb;
    const vec3 worldR = (inverse(V) * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(irrMap, worldN).rgb;
    const vec3 diffuse = irradiance * albedo;

    const float MaxReflectionLod = 4.0f;
    const vec3 prefilter = textureLod(refMap, worldR, roughness * MaxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    const vec3 specular = prefilter * (F * brdf.x + brdf.y);

    const vec3 Lenv = (kD * diffuse + specular) * ao;

    const vec3 eyeLightPos = (V * (-light.position)).xyz;
    const vec3 lightVec = eyeLightPos;//eyeLightPos - eyePosition;
    const vec3 eyeL = normalize(lightVec);
    const vec3 eyeH = normalize(eyeL + eyeV);

    const float NdotL = max(dot(eyeN, eyeL), 0.0f);
    
    const vec3 radiance = estimateRadiance(lightVec);

    const float D = distributionGGX(eyeN, eyeH, roughness);
    const float G = geometrySmith(eyeN, eyeV, eyeL, roughness);
    const vec3 numerator = D * G * F;
    const float denominator = 4.0f * NdotV * NdotL;
    const vec3 lightSpecular = numerator / max(denominator, 0.001);

    const vec3 Li = (kD * albedo / PI + lightSpecular) * radiance * NdotL;

    vec3 debugColor;
    float shadow = getCsmShadowCoeffDebug(dot(eyeN, eyeL), debugColor);

    fragColor = vec4(Li * shadow + Lenv, 1.0f);
    ////float shadow = getCsmShadowCoeffDebugDepth(abs(eyePosition.z), cosThetaRaw, albedo);
    //fragColor = vec4(vec3(ambient + albedo * shadow) * ao, 1.0f);
    //if (gl_FragCoord.x < 500 && gl_FragCoord.y < 500) {
    //    float x = gl_FragCoord.x / 500.0f;
    //    float y = gl_FragCoord.y / 500.0f;
    //    float d = texture(shadowMap, vec2(x, y)).r;
    //    fragColor.xyz = vec3(d / 100.0f);
    //}


    //fragColor = vec4(eyeNormal, 1.0f);
    //fragColor = vec4(vec3(bias), 1.0f);
//
    ////fragColor = vec4(eyeBitangent, 1.0f);
    //vec3 nn = getNormal(texCoord);
    //cosTheta = max(dot(nn, wi), 0.0f);
    //fragColor = vec4(radiance * cosTheta, 1.0f);
    //fragColor = vec4(getNormal(texCoord), 1.0f);
}