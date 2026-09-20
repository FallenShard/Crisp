#version 460 core
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/view.part.glsl"
#include "meshlet-culling.part.glsl"

struct Meshlet {
    uint vertexOffset;
    uint triangleOffset;
    uint vertexCount;
    uint triangleCount;
};

layout(local_size_x = 64) in;
layout(triangles, max_vertices = 64, max_primitives = 124) out;

layout(binding = 0, std430) buffer MeshletBuffer {
    Meshlet meshlets[];
};

layout(binding = 1, std430) buffer MeshletIndexBuffer {
    uint8_t meshletIndices[];
};

layout(binding = 2, std430) buffer MeshletVertices {
    uint meshletVertices[];
};

layout(binding = 3, scalar) buffer Vertices {
    vec3 vertices[];
};

layout(binding = 4) uniform View {
    ViewParameters view;
};

layout(binding = 5) buffer VertexAttributes {
    float vertexAttribs[];
};

taskPayloadSharedEXT MeshletTaskPayload payload;

// Per-vertex output attributes
perprimitiveEXT layout(location = 0) out vec3 outColors[];

const vec3 colors[4] = {vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f), vec3(1.0f, 1.0f, 0.0f)};

void main() {
    const uint meshletIndex = payload.meshletIndices[gl_WorkGroupID.x];
    Meshlet meshlet = meshlets[meshletIndex];
    SetMeshOutputsEXT(meshlet.vertexCount, meshlet.triangleCount);

    for (uint i = gl_LocalInvocationIndex; i < meshlet.vertexCount; i += gl_WorkGroupSize.x) {
        const uint index = meshletVertices[meshlet.vertexOffset + i];
        gl_MeshVerticesEXT[i].gl_Position = view.P * view.V * vec4(vertices[index], 1.0f);
        // outColors[i] = vec3(vertexAttribs[9 * index + 0], vertexAttribs[9 * index + 1], vertexAttribs[9 * index +
        // 2]);
    }

    for (uint i = gl_LocalInvocationIndex; i < meshlet.triangleCount; i += gl_WorkGroupSize.x) {
        const uint offset = meshlet.triangleOffset + 3 * i;
        gl_PrimitiveTriangleIndicesEXT[i] =
            uvec3(meshletIndices[offset], meshletIndices[offset + 1], meshletIndices[offset + 2]);
        outColors[i] = colors[meshletIndex % 4];
    }
}
