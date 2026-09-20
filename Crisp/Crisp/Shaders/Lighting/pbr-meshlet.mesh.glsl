#version 460 core
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/view.part.glsl"
#include "../Geometry/meshlet-culling.part.glsl"

layout(local_size_x = 64) in;
layout(triangles, max_vertices = 64, max_primitives = 124) out;

struct Meshlet {
    uint vertexOffset;
    uint triangleOffset;
    uint vertexCount;
    uint triangleCount;
};

struct TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout(std430, set = 2, binding = 0) readonly buffer Transforms {
    TransformPack transforms[];
};

layout(std430, set = 3, binding = 0) readonly buffer Meshlets {
    Meshlet meshlets[];
};

layout(std430, set = 3, binding = 1) readonly buffer MeshletVertices {
    uint meshletVertices[];
};

layout(std430, set = 3, binding = 2) readonly buffer MeshletTriangles {
    uint8_t meshletTriangles[];
};

layout(std430, set = 3, binding = 3) readonly buffer MeshletBoundsBuffer {
    MeshletBounds meshletBounds[];
};

layout(std430, set = 3, binding = 4) readonly buffer MeshletTransformIndices {
    uint meshletTransformIndex[];
};

layout(scalar, set = 3, binding = 5) readonly buffer Positions {
    vec3 positions[];
};

layout(scalar, set = 3, binding = 6) readonly buffer Attributes {
    float attributes[];
};

#include "pbr-meshlet-draw.part.glsl"

taskPayloadSharedEXT MeshletTaskPayload payload;

layout(location = 0) out vec3 eyeNormal[];
layout(location = 1) out vec2 outTexCoord[];
layout(location = 2) out vec3 eyePosition[];
layout(location = 3) out vec4 eyeTangent[];
layout(location = 4) out vec3 worldPos[];

const uint kAttributeStride = 9;

void main() {
    const uint meshletIndex = payload.meshletIndices[gl_WorkGroupID.x];
    const Meshlet meshlet = meshlets[meshletIndex];
    SetMeshOutputsEXT(meshlet.vertexCount, meshlet.triangleCount);

    const TransformPack transform = transforms[meshletTransformIndex[meshletIndex]];

    for (uint i = gl_LocalInvocationIndex; i < meshlet.vertexCount; i += gl_WorkGroupSize.x) {
        const uint vertexIndex = meshletVertices[meshlet.vertexOffset + i];
        const vec3 position = positions[vertexIndex];
        const uint attributeBase = vertexIndex * kAttributeStride;

        const vec3 normal =
            vec3(attributes[attributeBase + 0], attributes[attributeBase + 1], attributes[attributeBase + 2]);
        const vec2 texCoord = vec2(attributes[attributeBase + 3], attributes[attributeBase + 4]);
        const vec4 tangent = vec4(
            attributes[attributeBase + 5],
            attributes[attributeBase + 6],
            attributes[attributeBase + 7],
            attributes[attributeBase + 8]);

        gl_MeshVerticesEXT[i].gl_Position = transform.MVP * vec4(position, 1.0f);

        const vec3 transformedNormal = normalize((transform.N * vec4(normal, 0.0f)).xyz);
        const vec3 transformedTangent = mat3(transform.MV) * tangent.xyz;

        eyeNormal[i] = transformedNormal;
        outTexCoord[i] = texCoord;
        eyePosition[i] = (transform.MV * vec4(position, 1.0f)).xyz;
        eyeTangent[i] = vec4(
            normalize(transformedTangent - transformedNormal * dot(transformedNormal, transformedTangent)), tangent.w);
        worldPos[i] = vec3(transform.M * vec4(position, 1.0f));
    }

    for (uint i = gl_LocalInvocationIndex; i < meshlet.triangleCount; i += gl_WorkGroupSize.x) {
        const uint offset = meshlet.triangleOffset + 3 * i;
        gl_PrimitiveTriangleIndicesEXT[i] = uvec3(
            uint(meshletTriangles[offset]), uint(meshletTriangles[offset + 1]), uint(meshletTriangles[offset + 2]));
    }
}
