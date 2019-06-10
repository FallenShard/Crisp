#version 460 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 vsEyeNormal;
layout(location = 1) in vec3 eyePosition;
layout(location = 2) in vec3 eyeTangent;
layout(location = 3) in vec3 eyeBitangent;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

layout(set = 0, binding = 2) uniform Material
{
    vec3 albedo;
    float metallic;
    float roughness;
} mat;

layout(set = 1, binding = 0) uniform Light
{
    mat4 VP;
    mat4 V;
    mat4 P;
    vec4 position;
    vec3 spectrum;
} light;

layout (set = 2, binding = 0) uniform samplerCube irrMap;
layout (set = 2, binding = 1) uniform samplerCube refMap;
layout (set = 2, binding = 2) uniform sampler2D brdfLut;

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

const vec3 lightColor = vec3(23.47, 21.31, 20.79);

float calculateAttenuation(in vec3 lightVec)
{
    float dist = length(lightVec);
    return 1.0f / (dist * dist);
}

vec3 estimateRadiance(in vec3 lightVec)
{
    float attenuation = calculateAttenuation(lightVec);
    float distFactor = mix(1.0f, attenuation, light.position.w);
    return light.spectrum * distFactor;
}

void main()
{
    const vec3  albedo    = mat.albedo;
    const float roughness = mat.roughness;
    const float metallic  = mat.metallic;

    // Bring all the vectors needed for lighting in eye (view) space
    vec3 eyeN = normalize(vsEyeNormal);
    vec3 eyeL = (V * (-light.position)).xyz;
    vec3 eyeV = normalize(-eyePosition);
    vec3 eyeR = reflect(-eyeV, eyeN);

    // Reflectance at normal incidence
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NdotV = max(dot(eyeN, eyeV), 0.0f);

    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;

    vec3 worldN = (inverse(V) * vec4(eyeN, 0.0f)).rgb;
    vec3 worldR = (inverse(V) * vec4(eyeR, 0.0f)).rgb;
    vec3 irradiance = texture(irrMap, worldN).rgb;
    vec3 diffuse = irradiance * albedo;

    const float MaxReflectionLod = 4.0f;
    vec3 prefilter = textureLod(refMap, worldR, roughness * MaxReflectionLod).rgb;
    vec2 envBrdf = texture(brdfLut, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilter * (F * envBrdf.x + envBrdf.y);

    const float ao = 1.0f;
    vec3 ambient = (kD * diffuse + specular) * ao;

    fragColor = vec4(ambient, 1.0f);
    //vec3 lightVec = eyeLightPos;// eyeLightPos - eyePosition;
    //vec3 wi = normalize(lightVec);
    //float cosThetaSigned = dot(eyeNormal, wi);
    //float cosTheta = max(cosThetaSigned, 0.0f);
    //float attenuation = calculateAttenuation(lightVec);
    //vec3 radiance = estimateRadiance(lightVec);//lightColor * attenuation;
//
    //vec3 halfDir = normalize(wi + eyeViewDir);
//
    //float NdotV = max(dot(eyeNormal, eyeViewDir), 0.0f);
//
    //
    //vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
//
    //
//
    //
//
    //float NDF = distributionGGX(eyeNormal, halfDir, roughness);
    //float G   = geometrySmith(eyeNormal, eyeViewDir, wi, roughness);
    //vec3 numerator = NDF * G * F;
    //float denominator = 4.0f * max(dot(eyeNormal, viewDir), 0.0f) * max(dot(eyeNormal, wi), 0.0f);
    //vec3 specular = numerator / max(denominator, 0.001);
    //
    //vec3 Lo = (kD * albedo / PI + specular) * radiance * cosTheta;
}