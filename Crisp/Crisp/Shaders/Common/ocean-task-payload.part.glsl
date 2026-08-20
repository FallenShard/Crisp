#ifndef CRISP_OCEAN_TASK_PAYLOAD_GLSL
#define CRISP_OCEAN_TASK_PAYLOAD_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

#include "ocean-clipmap.part.glsl"

layout(constant_id = 0) const uint kOceanTilesPerTaskGroup = 32;

// Survivors are stored as offsets from the group's first tile, not absolute tile indices, so a byte is enough
// however many tiles a block splits into -- an absolute index needs more than eight bits past 256 tiles per block.
// The offset is bounded by kOceanTilesPerTaskGroup, which is also the task group size.
struct OceanTaskPayload {
    uint blockIndex;
    uint baseTile;
    uint8_t tileOffsets[kOceanTilesPerTaskGroup];
};

#endif // CRISP_OCEAN_TASK_PAYLOAD_GLSL
