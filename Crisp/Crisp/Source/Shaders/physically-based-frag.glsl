#version 450 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 eyePosition;
layout(location = 3) in vec3 eyeTangent;
layout(location = 4) in vec3 eyeBitangent;

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
layout(set = 0, binding = 7) uniform sampler2D envMap;

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

//const vec4 lightPosition = vec4(0.0f, 3.0f, 0.0f, 1.0f);
//const vec3 lightColor = vec3(5.0f, 5.0f, 4.5f);
const vec3 lightColor = vec3(23.47, 21.31, 20.79);

layout(push_constant) uniform LightPosition
{
    layout(offset = 0) vec4 lightPosition;
};

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

void main()
{
    vec3 albedo = texture(diffuseTex, texCoord).rgb;
    float roughness = texture(roughnessTex, texCoord).r;
    float metallic = texture(metallicTex, texCoord).r;

    vec3 eyeNormal = getNormal(texCoord);
    vec3 eyeLightPos = (V * lightPosition).xyz;

    vec3 viewDir = normalize(-eyePosition);

    vec3 lightVec = eyeLightPos - eyePosition;
    vec3 wi = normalize(lightVec);
    float cosTheta = max(dot(eyeNormal, wi), 0.0f);
    float attenuation = calculateAttenuation(lightVec);
    vec3 radiance = lightColor * attenuation;

    vec3 halfDir = normalize(wi + viewDir);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(halfDir, viewDir), 0.0f), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0f) - kS;
    kD *= 1.0f - metallic;

    float NDF = distributionGGX(eyeNormal, halfDir, roughness);
    float G   = geometrySmith(eyeNormal, viewDir, wi, roughness);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(eyeNormal, viewDir), 0.0f) * max(dot(eyeNormal, wi), 0.0f);
    vec3 specular = numerator / max(denominator, 0.001);

    vec3 Lo = (kD * albedo / PI + specular) * radiance * cosTheta;

    // ambient lighting (we now use IBL as the ambient term)
    kS = fresnelSchlickRoughness(max(dot(eyeNormal, viewDir), 0.0f), F0, roughness);
    kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    vec3 irradiance = texture(envMap, texCoord).rgb;
    vec3 diffuse      = irradiance * albedo;
    vec3 ambient = (kD * diffuse) * 0.1f;
    ambient = vec3(0.03) * albedo * 1.0f;

	float ao = texture(aoTex, texCoord).r;

    fragColor = vec4(vec3(ambient + Lo) * ao, 1.0f);
    //fragColor = vec4(eyeNormal, 1.0f);
    ////fragColor = vec4(vec3(roughness), 1.0f);
//
    ////fragColor = vec4(eyeBitangent, 1.0f);
    //vec3 nn = getNormal(texCoord);
    //cosTheta = max(dot(nn, wi), 0.0f);
    //fragColor = vec4(radiance * cosTheta, 1.0f);
    //fragColor = vec4(getNormal(texCoord), 1.0f);
}