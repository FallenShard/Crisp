#ifndef CRISP_PATH_TRACER_INSTANCE_GLSL
#define CRISP_PATH_TRACER_INSTANCE_GLSL

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTracedPositions {
    vec3 data[];
};

// Interleaved to match kPbrVertexFormat's second binding.
struct PathTracedVertexAttributes {
    vec3 normal;
    vec2 texCoord;
    vec4 tangent;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTracedAttributes {
    PathTracedVertexAttributes data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTracedTriangles {
    uvec3 data[];
};

struct AliasTableElement {
    float tau;
    uint j;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTracedAliasTable {
    AliasTableElement data[];
};

// Must match the kPathTracedInstance* constants in Scenes/PathTracer.hpp.
const uint kInstanceTwoSidedShading = 1u;

// Must match PathTracedInstance in Scenes/PathTracer.hpp. One record per TLAS instance; geometry lives in
// per-node buffers rather than one merged scene buffer, so the addresses travel with the instance.
struct PathTracedInstance {
    PathTracedPositions positions;
    PathTracedAttributes attributes;
    PathTracedTriangles triangles;
    PathTracedAliasTable aliasTable; // Null unless the shape is an area light.
    int materialIndex;
    int lightId;                // -1 unless the shape is an area light.
    uint materialTextureOffset; // kInvalidMaterialTextureOffset when the material is untextured.
    uint flags;
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer PathTracedInstances {
    PathTracedInstance data[];
};

#endif // CRISP_PATH_TRACER_INSTANCE_GLSL
