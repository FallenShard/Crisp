#version 460 core

#extension GL_EXT_mesh_shader : require

const uint kPatchVertexDim = 8;
const uint kMeshletVertices = kPatchVertexDim * kPatchVertexDim;
const uint kPatchQuadDim = kPatchVertexDim - 1;
const uint kMeshletPrimitives = 2 * kPatchQuadDim * kPatchQuadDim;

layout(local_size_x = 32) in;
layout(triangles, max_vertices = kMeshletVertices, max_primitives = kMeshletPrimitives) out;

layout(push_constant) uniform Params {
    vec2 viewportSize;
    float legPixels;
    uint meshletsPerRow;
}
params;

void main() {
    const uint meshletIndex = gl_WorkGroupID.x;
    const uint meshletColumn = meshletIndex % params.meshletsPerRow;
    const uint meshletRow = meshletIndex / params.meshletsPerRow;

    SetMeshOutputsEXT(kMeshletVertices, kMeshletPrimitives);

    const vec2 patchOrigin =
        vec2(float(meshletColumn * kPatchQuadDim), float(meshletRow * kPatchQuadDim)) * params.legPixels;

    for (uint v = gl_LocalInvocationIndex; v < kMeshletVertices; v += gl_WorkGroupSize.x) {
        const vec2 local = vec2(float(v % kPatchVertexDim), float(v / kPatchVertexDim)) * params.legPixels;
        const vec2 ndc = ((patchOrigin + local) / params.viewportSize) * 2.0f - 1.0f;
        gl_MeshVerticesEXT[v].gl_Position = vec4(ndc, 0.0f, 1.0f);
    }

    for (uint p = gl_LocalInvocationIndex; p < kMeshletPrimitives; p += gl_WorkGroupSize.x) {
        const uint quad = p / 2u;
        const uint quadColumn = quad % kPatchQuadDim;
        const uint quadRow = quad / kPatchQuadDim;
        const uint corner = quadColumn + quadRow * kPatchVertexDim;

        if ((p & 1u) == 0u) {
            gl_PrimitiveTriangleIndicesEXT[p] =
                uvec3(corner, corner + 1u, corner + kPatchVertexDim);
        } else {
            gl_PrimitiveTriangleIndicesEXT[p] =
                uvec3(corner + 1u, corner + kPatchVertexDim + 1u, corner + kPatchVertexDim);
        }
    }
}
