#ifndef CRISP_PBR_MESHLET_DRAW_GLSL
#define CRISP_PBR_MESHLET_DRAW_GLSL

layout(push_constant) uniform MeshletDrawParameters {
    layout(offset = 24) uint firstMeshlet;
    uint meshletCount;
    uint cullingEnabled;
}
meshletDraw;

#endif // CRISP_PBR_MESHLET_DRAW_GLSL
