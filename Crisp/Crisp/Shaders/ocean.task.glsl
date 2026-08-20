#version 460 core

#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "Common/ocean-draw.part.glsl"
#include "Common/ocean-task-payload.part.glsl"

// Same id as kOceanTilesPerTaskGroup, so the group size and the payload it fills cannot drift apart.
layout(local_size_x_id = 0) in;

// Must cover OCEAN_TILES_PER_BLOCK together with kOceanTilesPerTaskGroup; too small silently drops the tail tiles.
layout(constant_id = 1) const uint kOceanTaskGroupsPerBlock = 8;

// Wave height is Gaussian, so four standard deviations bounds a tile in practice.
layout(constant_id = 2) const float kOceanTileHeightSigmas = 4.0f;

layout(set = 0, binding = 0) uniform TransformPack {
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

taskPayloadSharedEXT OceanTaskPayload payload;

shared uint sharedSurvivorCount;

// Only ever culled when every corner falls outside the same plane, so the test never removes a tile
// that contributes a pixel. Corners behind the eye make the side-plane comparisons meaningless, so
// a straddling box is kept and only a wholly-behind one is dropped.
bool isBoxVisible(const vec3 boxMin, const vec3 boxMax) {
    uint inFront = 0;
    uint outLeft = 0;
    uint outRight = 0;
    uint outBottom = 0;
    uint outTop = 0;

    for (int i = 0; i < 8; ++i) {
        const vec3 corner = vec3(
            (i & 1) == 0 ? boxMin.x : boxMax.x,
            (i & 2) == 0 ? boxMin.y : boxMax.y,
            (i & 4) == 0 ? boxMin.z : boxMax.z);
        const vec4 clipPos = MVP * vec4(corner, 1.0f);
        if (clipPos.w <= 0.0f) {
            continue;
        }

        ++inFront;
        outLeft += clipPos.x < -clipPos.w ? 1 : 0;
        outRight += clipPos.x > clipPos.w ? 1 : 0;
        outBottom += clipPos.y < -clipPos.w ? 1 : 0;
        outTop += clipPos.y > clipPos.w ? 1 : 0;
    }

    if (inFront == 0) {
        return false;
    }
    if (inFront < 8) {
        return true;
    }
    return outLeft < 8 && outRight < 8 && outBottom < 8 && outTop < 8;
}

bool isTileVisible(const uint blockIndex, const uint tileIndex) {
    const uint tileX = tileIndex % OCEAN_TILES_X;
    const uint tileZ = tileIndex / OCEAN_TILES_X;
    const float halfBlock = 0.5f * float(clipmapBlockQuads);
    const vec2 minCell = vec2(float(tileX * OCEAN_TILE_QUADS_X), float(tileZ * OCEAN_TILE_QUADS_Z)) - halfBlock;
    const vec2 maxCell = minCell + vec2(float(OCEAN_TILE_QUADS_X), float(OCEAN_TILE_QUADS_Z));

    // Through the same placement the mesh shader uses, so the bound cannot drift from the geometry.
    const OceanClipmapVertex corner0 =
        placeOceanClipmapVertex(int(blockIndex), minCell, clipmapOrigin, clipmapFinestSpacing, clipmapBlockQuads);
    const OceanClipmapVertex corner1 =
        placeOceanClipmapVertex(int(blockIndex), maxCell, clipmapOrigin, clipmapFinestSpacing, clipmapBlockQuads);

    const float sigma = 1.0f / max(invRmsWaveHeight, 1e-6f);
    // The morph slides a vertex by up to one cell toward its even neighbour, and choppiness shears it
    // sideways by about as much as the wave is tall.
    const float lateralSlack = corner0.vertexSpacing + choppiness * kOceanTileHeightSigmas * sigma;

    const vec2 boxMinXZ = min(corner0.worldXZ, corner1.worldXZ) - lateralSlack;
    const vec2 boxMaxXZ = max(corner0.worldXZ, corner1.worldXZ) + lateralSlack;

    // Curvature drops the far edge of a distant tile by more than a kilometre, so the vertical bound
    // has to follow the bend rather than sit at +-4 sigma about zero.
    const vec2 outside = max(max(boxMinXZ - clipmapOrigin, clipmapOrigin - boxMaxXZ), vec2(0.0f));
    const vec2 farthest = max(abs(boxMinXZ - clipmapOrigin), abs(boxMaxXZ - clipmapOrigin));
    const float nearBend = dot(outside, outside) / (2.0f * planetRadius);
    const float farBend = dot(farthest, farthest) / (2.0f * planetRadius);

    const float waveSlack = kOceanTileHeightSigmas * sigma;
    const vec3 boxMin = vec3(boxMinXZ.x, -farBend - waveSlack, boxMinXZ.y);
    const vec3 boxMax = vec3(boxMaxXZ.x, -nearBend + waveSlack, boxMaxXZ.y);
    return isBoxVisible(boxMin, boxMax);
}

void main() {
    if (gl_LocalInvocationIndex == 0) {
        sharedSurvivorCount = 0;
    }
    barrier();

    const uint blockIndex = gl_WorkGroupID.x / kOceanTaskGroupsPerBlock;
    const uint groupInBlock = gl_WorkGroupID.x % kOceanTaskGroupsPerBlock;
    const uint baseTile = groupInBlock * kOceanTilesPerTaskGroup;
    const uint tile = baseTile + gl_LocalInvocationIndex;
    if (tile < OCEAN_TILES_PER_BLOCK && isTileVisible(blockIndex, tile)) {
        payload.tileOffsets[atomicAdd(sharedSurvivorCount, 1)] = uint8_t(gl_LocalInvocationIndex);
    }

    payload.blockIndex = blockIndex;
    payload.baseTile = baseTile;
    barrier();

    EmitMeshTasksEXT(sharedSurvivorCount, 1, 1);
}
