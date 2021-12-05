#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 eyeNormal;
layout(location = 1) out vec3 eyePosition;
layout(location = 2) out vec3 worldPosition;
layout(location = 3) out vec3 eyeTangent;
layout(location = 4) out vec3 eyeBitangent;

layout(set = 0, binding = 0) uniform TransformPack
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main()
{
    eyeNormal = normalize((N * vec4(normal, 0.0f)).xyz);
    eyePosition = (MV * vec4(position, 1.0f)).xyz;
    worldPosition = (M * vec4(position, 1.0f)).xyz;
    eyeTangent = normalize((N * vec4(tangent.xyz, 0.0f)).xyz);
    eyeBitangent = normalize((N * vec4(tangent.w * cross(normal, tangent.xyz), 0.0f)).xyz);

    gl_Position = MVP * vec4(position, 1.0f);
}