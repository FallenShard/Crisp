#version 450 core
#define PI 3.1415926535897932384626433832795

layout(location = 0) in vec3 eyeNormal;
layout(location = 1) in vec3 worldNormal;
layout(location = 2) in vec3 eyePosition;

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


float DistributionGGX(vec3 N, vec3 H, float a)
{
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

const vec4 lightPosition = vec4(2.0f, 2.0f, 2.0f, 1.0f);
const vec3 lightColor = vec3(5.0f, 5.0f, 4.5f);

void main()
{
    vec3 worldN = normalize(worldNormal);
    float u = 0.5f + atan(worldN.z, worldN.x) / 3.141592f * 0.5f;
    float v = 0.5f - asin(worldN.y) / 3.141592f;
    vec2 texCoord = vec2(u, v);

    vec3 diffuse = texture(diffuseTex, texCoord).rgb;
    float roughness = texture(roughnessTex, texCoord).r;
    float metallic = texture(metallicTex, texCoord).r;

    vec3 eyeN = normalize(eyeNormal);
    vec3 eyeLightPos = (V * lightPosition).xyz;

    vec3 viewDir = normalize(-eyePosition);

    vec3 lightVec = eyeLightPos - eyePosition;
    vec3 lightDir = normalize(lightVec);
    vec3 halfDir = normalize(lightDir + viewDir);
    float dist = length(lightVec);
    float attenuation = 1.0f / (dist * dist);
    vec3 radiance = lightColor * attenuation;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, diffuse, metallic);
    vec3 F = fresnelSchlick(max(0, dot(halfDir, viewDir)), F0);

    float NDF = DistributionGGX(eyeN, halfDir, roughness);
    float G   = GeometrySmith(eyeN, viewDir, lightDir, roughness);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(eyeN, viewDir), 0.0f) * max(dot(eyeN, lightDir), 0.0f);
    vec3 specular = numerator / max(denominator, 0.001);

    vec3 kS = F;
    vec3 kD = vec3(1.0f) - kS;
    kD *= 1.0f - metallic;

    float cosTheta = max(dot(eyeN, lightDir), 0.0f);

    vec3 Lo = (kD * diffuse / PI + specular) * radiance * cosTheta;

    vec3 ambient = 0.00f * diffuse;

    fragColor = vec4(vec3(ambient + Lo), 1.0f);
}