#ifndef CRISP_MESHLET_CULLING_GLSL
#define CRISP_MESHLET_CULLING_GLSL

// Mirrors crisp::MeshletBounds and crisp::isMeshletConeVisible, which the CPU-side tests check this against.
const uint kTaskWorkGroupSize = 32;

struct MeshletBounds {
    vec4 centerRadius;   // xyz: bounding sphere center, w: its radius.
    vec4 coneApex;       // xyz: normal cone apex, w: unused padding.
    vec4 coneAxisCutoff; // xyz: normal cone axis, w: cos(angle / 2).
};

struct MeshletTaskPayload {
    uint meshletIndices[kTaskWorkGroupSize];
};

bool isMeshletConeVisible(const MeshletBounds bounds, const vec3 cameraPosition) {
    const float coneCutoff = bounds.coneAxisCutoff.w;
    if (coneCutoff >= 1.0f) {
        return true;
    }

    const vec3 toApex = bounds.coneApex.xyz - cameraPosition;
    const float distanceSquared = dot(toApex, toApex);
    if (distanceSquared <= 0.0f) {
        return true;
    }

    return dot(toApex, bounds.coneAxisCutoff.xyz) < coneCutoff * sqrt(distanceSquared);
}

#endif // CRISP_MESHLET_CULLING_GLSL
