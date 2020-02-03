#version 460 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec3 eyePosition;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 fragColor;

const vec3 ndcMin = vec3(-1.0f, -1.0f, 0.0f);
const vec3 ndcMax = vec3(+1.0f, +1.0f, 1.0f);

// ----- Camera -----

layout(set = 0, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};



// ----- PBR Microfacet BRDF -------

layout(set = 0, binding = 2) uniform Material
{
    vec3 albedo;
    float metallic;
    float roughness;
} mat;

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num    = a2;
    float denom  = NdotH2 * (a2 - 1.0f) + 1.0f;
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

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ----- Environment Lighting -----

layout (set = 2, binding = 0) uniform samplerCube irrMap;
layout (set = 2, binding = 1) uniform samplerCube refMap;
layout (set = 2, binding = 2) uniform sampler2D brdfLut;

vec3 computeEnvRadiance(vec3 eyeN, vec3 eyeV, vec3 kD, vec3 albedo, vec3 F, float roughness, float ao)
{
    const vec3 worldN = (inverse(V) * vec4(eyeN, 0.0f)).rgb;
    const vec3 irradiance = texture(irrMap, worldN).rgb;
    const vec3 diffuse = irradiance * albedo;

    const float NdotV = max(dot(eyeN, eyeV), 0.0f);
    const vec3 eyeR = reflect(-eyeV, eyeN);
    const vec3 worldR = (inverse(V) * vec4(eyeR, 0.0f)).rgb;

    const float MaxReflectionLod = 4.0f;
    const vec3 prefilter = textureLod(refMap, worldR, roughness * MaxReflectionLod).rgb;
    const vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).xy;
    const vec3 specular = prefilter * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * ao;
}

// ----- Light -----

layout(set = 1, binding = 0) uniform Light
{
    mat4 VP;
    mat4 V;
    mat4 P;
    vec4 position;
    vec3 spectrum;
} light;

vec3 evalSpotLightRadiance()
{
    vec3 eyeO = (V * vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
    vec3 eyeL = (V * vec4(light.position.xyz, 1.0f)).xyz;
    vec3 coneDir = eyeO - eyeL;

    vec3 eyeP = eyePosition;
    vec3 lightToPos = eyeP - eyeL;

    float sim = dot(normalize(coneDir), normalize(lightToPos));
    if (sim < 0.7f)
        return vec3(0.0f);
    else if (sim < 0.8f)
        return vec3(smoothstep(0.7, 0.8, sim));//vec3((sim - 0.7f) / 0.1f);
    else
        return vec3(1.0f);
}

vec3 evalPointLightRadiance(out vec3 eyeL)
{
    const vec3 eyeLightPos = (V * vec4(light.position.xyz, 1.0f)).xyz;
    const vec3 eyePosToLight = eyeLightPos - eyePosition;
    const float dist = length(eyePosToLight);

    eyeL = eyePosToLight / dist;
    return light.spectrum / (dist * dist);
}

vec3 evalPointLightRadiance(in vec3 lightPos, in vec3 spectrum, out vec3 eyeL)
{
    const vec3 eyeLightPos = (V * vec4(lightPos, 1.0f)).xyz;
    const vec3 eyePosToLight = eyeLightPos - eyePosition;
    const float dist = length(eyePosToLight);

    eyeL = eyePosToLight / dist;
    return spectrum / (dist * dist);
}

vec3 evalDirectionalLightRadiance(out vec3 eyeL)
{
    eyeL = normalize((V * vec4(light.position.xyz, 0.0f)).xyz);
    return light.spectrum;
}

// ----- Cascaded Shadow Mapping -----

layout(set = 1, binding = 1) uniform sampler2DArray shadowMap;

layout(set = 1, binding = 2) uniform CascadeSplit
{
    vec4 lo;
    vec4 hi;
} cascadeSplit;

layout(set = 1, binding = 3) uniform LightTransforms
{
    mat4 VP[4];
} lightTransforms;

bool isInCascade(int cascadeIndex)
{
    vec4 lightSpacePos = lightTransforms.VP[cascadeIndex] * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
       return false;
    else
       return true;
}

float getShadowCoeff(float bias, mat4 LVP, int cascadeIndex, int pcfRadius)
{
    vec4 lightSpacePos = LVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
        return 0.0f;

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    ivec2 size = textureSize(shadowMap, 0).xy;
    vec2 texelSize = vec2(1) / size;

    float amount  = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++) {
        for (int j = -pcfRadius; j <= pcfRadius; j++) {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            float shadowMapDepth = texture(shadowMap, vec3(tc, cascadeIndex)).r;
            amount += shadowMapDepth < ndcPos.z - bias ? 0.0f : 1.0f;
        }
    }

    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    return amount / numSamples;
}

// ----- Standard Shadow Mapping -----

layout(set = 1, binding = 4) uniform sampler2D pointShadowMap;
layout(set = 1, binding = 5) uniform LightTransforms2
{
    mat4 VP[4];
} pointLight;

struct ManyLightDescriptor
{
    vec3 position;
    float radius;
    vec3 spectrum;
    float padding;
};

layout(set = 1, binding = 6) uniform ManyLights
{
    ManyLightDescriptor lights[1024];
} many;

layout(set = 1, binding = 7, rg32ui) uniform readonly uimage2D lightGrid;
layout(set = 1, binding = 8) buffer LightIndexList
{
    uint lightIndexList[];
};

layout(push_constant) uniform LightTest
{
    layout(offset = 0) int switchMethod;
};

float getShadowCoeff(float bias, mat4 LVP, int pcfRadius)
{
    vec4 lightSpacePos = LVP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;

    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
        return 0.0f;

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;

    ivec2 size = textureSize(shadowMap, 0).xy;
    vec2 texelSize = vec2(1) / size;

    const float numSamples = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    float amount  = 0.0f;
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            float shadowMapDepth = texture(pointShadowMap, tc).r;
            amount += shadowMapDepth < (lightSpacePos.z - bias) / lightSpacePos.w ? 0.0f : 1.0f;
        }
    }

    return amount / numSamples;
}

// ----- Variance Shadow Mapping -----

vec3 getVsmCoeff()
{
    vec4 lvpPos = light.VP * vec4(worldPos, 1.0f);
    vec3 ndcPos = lvpPos.xyz / lvpPos.w;
    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
        return vec3(1.0f);

    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;
    ivec2 size = textureSize(pointShadowMap, 0).xy;
    vec2 texelSize = vec2(1.0f) / size;

    int pcfRadius = 5;
    vec2 moments = vec2(0.0f);
    for (int i = -pcfRadius; i <= pcfRadius; i++)
    {
        for (int j = -pcfRadius; j <= pcfRadius; j++)
        {
            vec2 tc = texCoord + vec2(i, j) * texelSize;
            moments += texture(pointShadowMap, tc).rg;
        }
    }

    moments /= (2 * pcfRadius + 1) * (2 * pcfRadius + 1);

    vec4 lvPos = light.V * vec4(worldPos, 1.0f);
    float fragDepth = -lvPos.z;
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;

    float variance = E_x2 - Ex_2;
    float mD = fragDepth - moments.x;
    float mD_2 = mD * mD;
    float p = variance / (variance + mD_2);
    float lit = max( p, float(fragDepth <= moments.x) );
    return vec3(lit);
}

// Check-in-bounds based
float getCsmShadowCoeffDebug(float cosTheta, out vec3 color)
{
    color = vec3(0.0f, 0.0f, 1.0f);
    int cascadeIndex = 3;
    if (isInCascade(0))
    {
        color = vec3(1.0f, 0.0f, 0.0f);
        cascadeIndex = 0;
    }
    else if (isInCascade(1))
    {
        color = vec3(1.0f, 1.0f, 0.0f);
        cascadeIndex = 1;
    }
    else if (isInCascade(2))
    {
        color = vec3(0.0f, 1.0f, 0.0f);
        cascadeIndex = 2;
    }

    color *= 0.3f;

    float bias = 0.005 * tan(acos(cosTheta)); // cosTheta is dot( n,l ), clamped between 0 and 1
    bias = clamp(bias, 0.0f, 0.01f);
    return getShadowCoeff(bias, lightTransforms.VP[cascadeIndex], cascadeIndex);
}



void main()
{
    // Basic shading geometry
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    // Primary light source radiance
    vec3 eyeL; // light direction, out param
    const vec3 Le = evalPointLightRadiance(eyeL);
    const float NdotL = max(dot(eyeN, eyeL), 0.0f);

    // Material properties
    const vec3  albedo    = mat.albedo;
    const float roughness = mat.roughness;
    const float metallic  = mat.metallic;
    const float ao        = 1.0f;

    // BRDF diffuse (Light source independent)
    const vec3 F0 = mix(vec3(0.04), albedo, metallic);
    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    const vec3 diffuse = kD * albedo / PI;

    // BRDF specularity (Light source dependent)
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float D = distributionGGX(eyeN, eyeH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);

    const vec3 Li = (diffuse + specularity) * Le * NdotL;
    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, albedo, F, roughness, ao);
    

    float bias = 0.01f * tan(acos(NdotL));
    //bias = clamp(bias, 0.0f, 0.01f);
    //const float shadow = getShadowCoeff(bias, pointLight.VP[0], 3);
    //const float shadow = getVsmCoeff();

    vec3 debugColor;
    //const float shadow = getCsmShadowCoeffDebug(, debugColor);

    vec3 acc = vec3(0.0f);

    if (switchMethod == 0)
    {
        for (int i = 0; i < 1024; ++i)
            {
            vec3 Le = evalPointLightRadiance(many.lights[i].position, many.lights[i].spectrum, eyeL);
            acc += max(dot(eyeN, eyeL), 0.0f) * Le;
            }
    }
    else
    {
        uvec2 lightIndexData = uvec2(imageLoad(lightGrid, ivec2(gl_FragCoord.xy) / ivec2(16)).xy);
            for (uint i = 0; i < lightIndexData[1]; ++i)
            {
               uint idx = lightIndexList[lightIndexData[0] + i];
               vec3 Le = evalPointLightRadiance(many.lights[idx].position, many.lights[idx].spectrum, eyeL);
               acc += max(dot(eyeN, eyeL), 0.0f) * Le;
            }
    }

    fragColor = vec4(Lenv * 0.0f + Li * getVsmCoeff(), 1.0f);

    fragColor.rgb = acc;
    //fragColor = vec4(vec3(shadow), 1.0f);
}