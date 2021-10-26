#version 450 core
#define PI 3.1415926535897932384626433832795
layout(location = 0) out vec4 finalColor;

layout(location = 0) in vec3 eyePosition;
layout(location = 1) in vec3 eyeNormal;
layout(location = 2) in vec3 worldNormal;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ----- Camera -----
layout(set = 1, binding = 0) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};


layout (set = 1, binding = 1) uniform samplerCube irrMap;
layout (set = 1, binding = 2) uniform samplerCube refMap;
layout (set = 1, binding = 3) uniform sampler2D brdfLut;

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

// -----
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom  = NdotH2 * (a2 - 1.0f) + 1.0f;
    denom        = PI * denom * denom;

    return a2 / denom;
}

float geometrySchlickGGX(float NdotVec, float roughness)
{
    float r = roughness + 1.0f;
    float k = r * r / 8.0f;
    float denom = NdotVec * (1.0f - k) + k;
    return NdotVec / denom;
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

void main()
{
    const vec3 eyeN = normalize(eyeNormal);
    const vec3 eyeV = normalize(-eyePosition);
    const float NdotV = max(dot(eyeN, eyeV), 0.0f);

    const vec3 lightDir = normalize(vec3(-1.0f, -1.0f, 0.0f));
    const vec3 eyeL = normalize(vec3(V * vec4(lightDir, 0.0f)));
    const float NdotL = max(dot(eyeN, -eyeL), 0.0f);

    vec3 ocean_color = pow(vec3(0,55,87) / 255.0, vec3(2.2));
    // Material properties
    const vec3  albedo    = ocean_color;
    const float roughness = 0.0;
    const float metallic  = 0.0;

    const vec3 F0 = mix(vec3(0.02), albedo, metallic);
    const vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    const vec3 kS = F;
    const vec3 kD = (1.0f - kS) * (1.0f - metallic);
    const vec3 diffuse = kD * albedo / PI;

   
    //vec3 light_color = pow(vec3(185,240,254) / 255.0, vec3(2.2));
    //vec3 color = mix(ocean_color, light_color, fresnel * fresnel);////

    const vec3 worldN = normalize(worldNormal);

    const vec3 Le = vec3(5.0f);
    
    // BRDF specularity (Light source dependent)
    const vec3 eyeH = normalize(eyeL + eyeV);
    const float D = distributionGGX(eyeN, eyeH, roughness);
    const float G = geometrySmith(NdotV, NdotL, roughness);
    const vec3 specularity = D * G * F / max(4.0f * NdotV * NdotL, 0.001);

    const vec3 Li = (diffuse + specularity) * Le * NdotL;
    const vec3 Lenv = computeEnvRadiance(eyeN, eyeV, kD, ocean_color, F, 0.0f, 1.0f);
    
    finalColor = vec4(vec3(Lenv), 1.0f);
}