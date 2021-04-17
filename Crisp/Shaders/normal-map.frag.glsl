#version 450 core

layout(location = 0) in vec3 eyePos;
layout(location = 1) in vec3 eyeNormal;
layout(location = 2) in vec3 worldPos;
layout(location = 3) in vec2 outTexCoord;
layout(location = 4) in vec3 eyeTangent;
layout(location = 5) in vec3 eyeBitangent;

layout(location = 0) out vec4 finalColor;

layout(set = 0, binding = 0) uniform TransformPack
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform Camera
{
    mat4 V;
    mat4 P;
    vec2 screenSize;
    vec2 nearFar;
};

//layout(set = 1, binding = 1) uniform sampler2D vsm;

layout(set = 1, binding = 0) uniform sampler2D normalMap;
layout(set = 1, binding = 1) uniform sampler2D diffuseMap;
layout(set = 1, binding = 2) uniform sampler2D specularMap;
//
//layout(set = 2, binding = 0) uniform Light
//{
//    mat4 VP;
//    mat4 V;
//    mat4 P;
//    vec4 position;
//    vec3 spectrum;
//} light;
//
//layout(set = 2, binding = 1) uniform sampler2D shadowMap;

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

//float getShadowCoeff()
//{
//   vec4 lightSpacePos = light.VP * vec4(worldPos, 1.0f);
//   vec3 ndcPos = lightSpacePos.xyz / lightSpacePos.w;
//
//   if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
//       return 1.0f;
//
//   vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;
//   float shadowMapDepth = texture(shadowMap, texCoord).r;
//
//   return shadowMapDepth < ndcPos.z ? 0.0f : 1.0f;
//}

//float getVSMCoeff() {
//    vec4 lightViewPos = pushConst.LV * vec4(worldPos, 1.0f);
//    float lightViewDist = -lightViewPos.z;
//
//    vec4 clipPos = pushConst.LP * lightViewPos;
//    vec3 ndcPos = clipPos.xyz / clipPos.w;
//    if (any(lessThan(ndcPos, ndcMin)) || any(greaterThan(ndcPos, ndcMax)))
//        return 0.2f;
//
//    vec2 texCoord = ndcPos.xy * 0.5f + 0.5f;
//    vec2 moments = texture(vsm, texCoord).rg;
//
//    if (lightViewDist <= moments.x)
//        return 1.0f;
//
//    float variance = moments.y - moments.x * moments.x;
//    variance = max(variance, 0.002);
//    float t = lightViewDist - moments.x;
//    float p = variance / (variance + t * t);
//    return p * 0.8f + 0.2f;
//}

// vec3 shadowEstimate(vec3 worldPos, float bias, in sampler2D sMap, in mat4 lvp)
// {
//     vec4 lsPos = lvp * vec4(worldPos, 1.0f);
//     vec4 lsPos1 = LVP[1] * vec4(worldPos, 1.0f);
//     vec4 lsPos2 = LVP[2] * vec4(worldPos, 1.0f);
//     vec4 lsPos3 = LVP[3] * vec4(worldPos, 1.0f);

//     // eyeDist = abs(eyePos.zzzz);
//     // weight = vec4(eyeDist > splitNear) * vec4(eyeDist < splitFar);

//     // lsPos = lsPos0 * weight.x + lsPos1 * weight.y + lsPos2 * weight.z + lsPos3 * weight.w;


//     vec3 projPos = lsPos.xyz / lsPos.w;

//     if (any(lessThan(projPos, ndcMin)) || any(greaterThan(projPos, ndcMax)))
//         return vec3(1.0f);

//     //if (projPos.x < -1.0f || projPos.x > 1.0f || projPos.y < -1.0f || projPos.y > 1.0f)
//     //	return vec3(1.0f);//return vec3(10000.0f, 0.0f, 0.0f);
//     //
//     //if (projPos.z > 1.0f)
//     //	return vec3(0.0f, 10000.0f, 0.0f);
//     //
//     //if (projPos.z < 0.0f)
//     //	return vec3(10000.0f, 10000.0f, 0.0f);
//     //

//     ivec2 texSize = textureSize(sMap, 0);
//     vec2 texelSize = vec2(1.0f) / texSize;
//     int radius = 3;
//     float taps = (2 * radius + 1) * (2 * radius + 1);
//     float sum = 0.0f;
//     for (int i = -radius; i <= radius; i++)
//     {
//         for (int j = -radius; j <= radius; j++)
//         {
//             vec2 texCoord = projPos.xy * 0.5f + 0.5f + vec2(i, j) * texelSize;
//             float shadowDepth = texture(sMap, texCoord).r;
//             if (projPos.z > shadowDepth + bias)
//                 sum += 1.0f;
//         }
//     }

//     //float shadowDepth = texture(sMap, projPos.xy * 0.5f + 0.5f).r;
//     //if (projPos.z > shadowDepth + bias)
//     //    return vec3(0.2f);

//     return vec3(1.0f - 0.8f * sum / taps);
// }

// vec3 shadowDebug(vec3 worldPos)
// {
//     vec4 lsPos = LVP[0] * vec4(worldPos, 1.0f);
//     vec3 projPos = lsPos.xyz;// / lsPos.w;

//     if (projPos.x < -1.0f || projPos.x > 1.0f || projPos.y < -1.0f || projPos.y > 1.0f)
//         return vec3(10000.0f, 0.0f, 0.0f);

//     if (projPos.z > 1.0f)
//         return vec3(10000.0f, 0.0f, 10000.0f);

//     if (projPos.z < 0.0f)
//         return vec3(10000.0f, 10000.0f, 0.0f);

//     return vec3(0.0f, 10000.0f, 0.0f);
// }

// vec3 getColor(vec3 pos)
// {
//     float dist = abs(pos.z);
//     if (dist > 20.0f)
//         return vec3(0.75, 0.15, 0.15) * InvPI;
//     else if (dist > 10.0f)
//         return vec3(0.75, 0.75, 0.15) * InvPI;
//     else if (dist > 5.0f)
//         return vec3(0.15, 0.75, 0.15) * InvPI;
//     else
//         return vec3(0.15, 0.15, 0.75) * InvPI;
// }

//vec3 Li(out vec3 wi)
//{
//    if (light.position.w == 0.0f)
//    {
//        wi = -vec3(V * light.position);
//        return light.spectrum;
//    }
//    else
//    {
//        wi = vec3(V * light.position) - eyePos;
//        float dist2 = dot(wi, wi);
//        return light.spectrum * InvFourPI / dist2;
//    }
//}

void main()
{
    vec3 normal  = normalize(eyeNormal);
    vec3 tangent = normalize(eyeTangent);
    vec3 bitangent = normalize(eyeBitangent);

    // Gram-Schmidt reorthogonalization
    tangent   = normalize(tangent   - normal * dot(tangent, normal));
    bitangent = normalize(bitangent - normal * dot(bitangent, normal) - tangent * dot(bitangent, tangent));
    mat3 TBN = mat3(tangent, bitangent, normal);

    vec2 texCoord = outTexCoord * 1;

    //// In tangent space
    normal = texture(normalMap, texCoord).rgb;
    normal = normalize(normal * 2.0f - 1.0f);
    normal = TBN * normal;

    vec3 lightPos = vec3(0.0f, 1.0f, 1.0f);
    vec3 posToLight = lightPos - worldPos;
    float distSq = dot(posToLight, posToLight);
    vec3 lightDir = normalize(posToLight);
    vec3 Le = vec3(10.0f) / distSq;

    vec3 wi = (V * vec4(lightDir, 0.0f)).xyz; // light direction in view space

    // Diffuse
    vec3 albedo = texture(diffuseMap, texCoord).rgb;
    float cosTheta = max(dot(wi, normal), 0.0f);
    vec3 diffuse = cosTheta * Le * albedo / PI;

    // Specular
    vec3 specCoeff = texture(specularMap, texCoord).rgb;
    vec3 viewDir = normalize(-eyePos);
    vec3 reflectDir = reflect(-wi, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 50.0f);
    vec3 specular = Le * spec * specCoeff;

    finalColor = vec4(vec3(1.0f, 0.2f, 0.2f), 1.0f);
    finalColor.rgb = vec3(0.0f);
    finalColor.rgb += diffuse;
    finalColor.rgb += specular;
}