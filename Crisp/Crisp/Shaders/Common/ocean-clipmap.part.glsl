#ifndef CRISP_OCEAN_CLIPMAP_GLSL
#define CRISP_OCEAN_CLIPMAP_GLSL

// Camera-centred nested rings. Level 0 is a full 4x4 arrangement of blocks; every level above it is
// the same 4x4 with the inner 2x2 dropped and the spacing doubled, so it covers exactly the annulus
// the level below vacated. Mirrors OceanClipmap in Crisp/Models/Ocean.hpp.
#define OCEAN_CLIPMAP_LEVEL0_BLOCKS 16
#define OCEAN_CLIPMAP_RING_BLOCKS 12

// The outer twelve cells of the 4x4, row major. Level 0 uses all sixteen.
const int kOceanClipmapRingCells[OCEAN_CLIPMAP_RING_BLOCKS] = int[OCEAN_CLIPMAP_RING_BLOCKS](
    0, 1, 2, 3, 4, 7, 8, 11, 12, 13, 14, 15);

// Fraction of a level's half-extent at which the morph toward the parent grid starts. Only the value
// it reaches at the boundary matters for correctness, and that has to be exactly 1.
const float kOceanClipmapMorphStart = 0.75f;

// Meshlet packing. Both tile a 126-quad block exactly and emit the same 31752 triangles per block; they differ
// only in how those are grouped. 0: 9x7 quads, 80 vertices and exactly 126 primitives -- primitive indices then
// fill three 128-byte blocks with two bytes spare, at the cost of overshooting the 64-vertex recommendation.
// 1: 7x7 quads, exactly 64 vertices but only 98 primitives. Must match OceanClipmap::blockQuads = 126, and
// kOceanTaskGroupsPerBlock in OceanScene.cpp has to cover the resulting tile count.
#define OCEAN_TILE_CONFIG 0

#if OCEAN_TILE_CONFIG == 0
#define OCEAN_TILE_QUADS_X 9
#define OCEAN_TILE_QUADS_Z 7
#define OCEAN_TILE_VERTICES 80
#define OCEAN_TILE_PRIMITIVES 126
#define OCEAN_TILES_X 14
#define OCEAN_TILES_Z 18
#define OCEAN_TILES_PER_BLOCK 252
#else
#define OCEAN_TILE_QUADS_X 7
#define OCEAN_TILE_QUADS_Z 7
#define OCEAN_TILE_VERTICES 64
#define OCEAN_TILE_PRIMITIVES 98
#define OCEAN_TILES_X 18
#define OCEAN_TILES_Z 18
#define OCEAN_TILES_PER_BLOCK 324
#endif

// Tiles per task group and task groups per block are specialization constants set from OceanScene.cpp; see
// ocean-task-payload.part.glsl. They partition the work rather than describe the geometry, so they are the only
// values here the host may choose -- max_vertices and max_primitives need literals.

struct OceanClipmapVertex {
    vec2 worldXZ;
    float vertexSpacing;
};

// `gridPos` is the block template's position in cells, centred on the block, so it runs over
// [-blockQuads/2, blockQuads/2] on both axes and is integral.
OceanClipmapVertex placeOceanClipmapVertex(
    const int instanceIndex,
    const vec2 gridPos,
    const vec2 origin,
    const float finestSpacing,
    const int blockQuads) {
    const bool isLevel0 = instanceIndex < OCEAN_CLIPMAP_LEVEL0_BLOCKS;
    const int ringIndex = instanceIndex - OCEAN_CLIPMAP_LEVEL0_BLOCKS;
    const int level = isLevel0 ? 0 : 1 + ringIndex / OCEAN_CLIPMAP_RING_BLOCKS;
    const int cell = isLevel0 ? instanceIndex : kOceanClipmapRingCells[ringIndex % OCEAN_CLIPMAP_RING_BLOCKS];

    const float spacing = finestSpacing * exp2(float(level));
    const vec2 blockCenter = (vec2(float(cell % 4), float(cell / 4)) - 1.5f) * float(blockQuads);

    // Integral, and spans [-2 * blockQuads, 2 * blockQuads] over the whole level.
    const vec2 cellCoord = blockCenter + gridPos;

    const float halfExtent = 2.0f * float(blockQuads);
    const float radius = max(abs(cellCoord.x), abs(cellCoord.y)) / halfExtent;
    const float alpha = smoothstep(kOceanClipmapMorphStart, 1.0f, radius);

    // Odd vertices slide onto their even neighbour, so the level's boundary row lands exactly on the
    // next level's grid. Without it every other vertex along a seam is a T-junction, and the seam
    // cracks as soon as the two sides displace by different amounts. mod() floors, so the target is
    // the same neighbour on both sides of the origin and adjacent blocks cannot disagree.
    const vec2 morphed = cellCoord - mod(cellCoord, 2.0f) * alpha;

    OceanClipmapVertex result;
    result.worldXZ = origin + morphed * spacing;
    // The morph is a continuous halving of the sample density, so the band weights have to follow it
    // or the geometry and the shading disagree about which cascades are present across the seam.
    result.vertexSpacing = spacing * mix(1.0f, 2.0f, alpha);
    return result;
}

#endif // CRISP_OCEAN_CLIPMAP_GLSL
