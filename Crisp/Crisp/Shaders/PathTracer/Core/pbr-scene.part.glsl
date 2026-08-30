#ifndef CRISP_PATH_TRACER_PBR_SCENE_GLSL
#define CRISP_PATH_TRACER_PBR_SCENE_GLSL

// Material-specific parameters. Must match PbrMaterialParams in Materials/PbrMaterial.hpp and the copy in
// Shaders/pbr.frag.glsl -- one material table feeds the raster and the path-traced view.
struct PbrMaterialParameters {
    vec3 baseColor;
    float baseWeight;

    vec3 specularColor;
    float specularWeight;

    vec3 emissionColor;
    float emissionLuminance;

    vec2 uvScale;
    float baseMetalness;
    float baseDiffuseRoughness;

    float specularRoughness;
    float specularIor;
    float normalScale;
    float aoStrength;

    uint samplerIndex;
    uint baseColorTex;
    uint normalTex;
    uint ormTex;

    uint emissionTex;
    float geometryOpacity;
    float alphaCutoff;
    uint flags;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PbrPositions {
    vec3 data[];
};

// Interleaved to match kPbrVertexFormat's second binding; only the normal is read for now.
struct PbrVertexAttributes {
    vec3 normal;
    vec2 texCoord;
    vec4 tangent;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PbrVertexAttributeBuffer {
    PbrVertexAttributes data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PbrTriangles {
    uvec3 data[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer PbrMaterials {
    PbrMaterialParameters data[];
};

// Must match PathTracedInstance in Scenes/PathTracedView.hpp. One record per TLAS instance; geometry lives in
// per-node buffers rather than one merged scene buffer, so the addresses travel with the instance.
struct PathTracedInstance {
    PbrPositions positions;
    PbrVertexAttributeBuffer attributes;
    PbrTriangles triangles;
    uint materialIndex;
    uint pad0;
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer PathTracedInstances {
    PathTracedInstance data[];
};

// Must match PathTracedViewAddresses in Scenes/PathTracedView.hpp.
layout(push_constant, scalar) uniform PathTracedViewAddresses {
    PathTracedInstances instances;
    PbrMaterials materials;
}
scene;

#endif // CRISP_PATH_TRACER_PBR_SCENE_GLSL
