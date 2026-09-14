#version 460 core

#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/ocean-draw.part.glsl"
#include "../Common/ocean-task-payload.part.glsl"

layout(local_size_x = 32) in;
layout(triangles, max_vertices = OCEAN_TILE_VERTICES, max_primitives = OCEAN_TILE_PRIMITIVES) out;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform sampler2DArray packedDisplacementMap;

taskPayloadSharedEXT OceanTaskPayload payload;

layout(location = 0) out vec3 eyePosition[];
layout(location = 1) out vec2 oceanWorldXZ[];
layout(location = 2) out float vertexSpacing[];

void main() {
    SetMeshOutputsEXT(OCEAN_TILE_VERTICES, OCEAN_TILE_PRIMITIVES);

    const uint blockIndex = payload.blockIndex;
    const uint tileIndex = payload.baseTile + uint(payload.tileOffsets[gl_WorkGroupID.x]);
    const uint tileX = tileIndex % OCEAN_TILES_X;
    const uint tileZ = tileIndex / OCEAN_TILES_X;
    const vec2 tileOriginCell =
        vec2(float(tileX * OCEAN_TILE_QUADS_X), float(tileZ * OCEAN_TILE_QUADS_Z)) - 0.5f * float(clipmapBlockQuads);

    const float fftSize = float(textureSize(packedDisplacementMap, 0).x);

    for (uint v = gl_LocalInvocationIndex; v < OCEAN_TILE_VERTICES; v += gl_WorkGroupSize.x) {
        const vec2 gridPos = tileOriginCell + vec2(float(v % (OCEAN_TILE_QUADS_X + 1)), float(v / (OCEAN_TILE_QUADS_X + 1)));
        const OceanClipmapVertex clipmapVertex =
            placeOceanClipmapVertex(int(blockIndex), gridPos, clipmapOrigin, clipmapFinestSpacing, clipmapBlockQuads);

        vec3 displaced = vec3(clipmapVertex.worldXZ.x, 0.0f, clipmapVertex.worldXZ.y);
        for (int c = 0; c < OCEAN_CASCADE_COUNT; ++c) {
            const float weight = oceanBandResolveWeight(cascadeWavelengths[c], clipmapVertex.vertexSpacing);
            if (weight <= 0.0f) {
                continue;
            }

            const vec3 uv = oceanCascadeUv(clipmapVertex.worldXZ, cascadeSizes[c], fftSize, c);
            const vec4 displacement = textureLod(packedDisplacementMap, uv, 0.0f);
            displaced += weight * vec3(-choppiness * displacement.g, displacement.r, -choppiness * displacement.b);
        }

        const vec2 fromCamera = clipmapVertex.worldXZ - clipmapOrigin;
        displaced.y -= dot(fromCamera, fromCamera) / (2.0f * planetRadius);

        oceanWorldXZ[v] = clipmapVertex.worldXZ;
        vertexSpacing[v] = clipmapVertex.vertexSpacing;
        eyePosition[v] = (MV * vec4(displaced, 1.0f)).xyz;
        gl_MeshVerticesEXT[v].gl_Position = MVP * vec4(displaced, 1.0f);
    }

    for (uint p = gl_LocalInvocationIndex; p < OCEAN_TILE_PRIMITIVES; p += gl_WorkGroupSize.x) {
        const uint quad = p / 2;
        const uint quadX = quad % OCEAN_TILE_QUADS_X;
        const uint quadZ = quad / OCEAN_TILE_QUADS_X;
        const uint corner = quadZ * (OCEAN_TILE_QUADS_X + 1) + quadX;
        const uint nextX = corner + 1;
        const uint nextZ = corner + OCEAN_TILE_QUADS_X + 1;
        const uint nextXZ = nextZ + 1;

        // Same split and winding createGridMesh emits, so both draw paths rasterize the same
        // triangles and can be compared pixel for pixel.
        gl_PrimitiveTriangleIndicesEXT[p] =
            (p % 2) == 0 ? uvec3(corner, nextXZ, nextX) : uvec3(corner, nextZ, nextXZ);
    }
}
