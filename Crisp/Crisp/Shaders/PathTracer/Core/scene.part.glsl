#ifndef CRISP_PATH_TRACER_SCENE_GLSL
#define CRISP_PATH_TRACER_SCENE_GLSL

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceVertices {
    vec3 data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceNormals {
    vec3 data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceTexCoords {
    vec2 data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceTriangles {
    uvec3 data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceInstances {
    InstanceProperties data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceMaterials {
    BrdfParameters data[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceLights {
    LightParameters data[];
};

struct AliasTableElement {
    float tau;
    uint j;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PathTraceAliasTable {
    AliasTableElement data[];
};

#ifndef CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_CDF_TYPE_GLSL
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer EnvironmentCdf { float data[]; };
#endif

// Must match RayTracingSceneAddresses in Scenes/RayTracingSceneData.hpp.
layout(push_constant, scalar) uniform RayTracingSceneAddresses {
    PathTraceVertices vertices;
    PathTraceNormals normals;
    PathTraceTexCoords texCoords;
    PathTraceTriangles triangles;
    PathTraceInstances instances;
    PathTraceMaterials materials;
    PathTraceLights lights;
    PathTraceAliasTable aliasTable;
    EnvironmentCdf environmentCdf;
}
scene;

#endif // CRISP_PATH_TRACER_SCENE_GLSL
