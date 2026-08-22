#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 eyeNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 eyePosition;
layout(location = 3) out vec4 eyeTangent;
layout(location = 4) out vec3 worldPos;

layout(set = 2, binding = 0) uniform Transforms {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

void main() {
    gl_Position = MVP * vec4(position, 1.0f);
    eyeNormal = normalize((N * vec4(normal, 0.0f)).xyz);

    const mat3 modelView = mat3(MV);
    const vec3 transformedTangent = modelView * tangent.xyz;
    eyeTangent.xyz = normalize(transformedTangent - eyeNormal * dot(eyeNormal, transformedTangent));
    // Mirroring is not supported.
    eyeTangent.w = tangent.w;
    eyePosition = (MV * vec4(position, 1.0f)).xyz;

    outTexCoord = texCoord;
    worldPos = vec3(M * vec4(position, 1.0f));
}
