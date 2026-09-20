#version 460 core
#extension GL_EXT_mesh_shader : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/view.part.glsl"
#include "meshlet-culling.part.glsl"

layout(local_size_x = kTaskWorkGroupSize) in;

layout(binding = 4) uniform View {
    ViewParameters view;
};

layout(binding = 6, std430) readonly buffer MeshletBoundsBuffer {
    MeshletBounds meshletBounds[];
};

layout(push_constant) uniform CullParameters {
    uint meshletCount;
    uint cullingEnabled;
}
cullParameters;

taskPayloadSharedEXT MeshletTaskPayload payload;

shared uint visibleMeshletCount;

void main() {
    if (gl_LocalInvocationIndex == 0) {
        visibleMeshletCount = 0;
    }
    barrier();

    const uint meshletIndex = gl_GlobalInvocationID.x;
    const bool withinRange = meshletIndex < cullParameters.meshletCount;

    const vec3 cameraPosition = view.invV[3].xyz;
    const bool visible = withinRange &&
        (cullParameters.cullingEnabled == 0 || isMeshletConeVisible(meshletBounds[meshletIndex], cameraPosition));

    if (visible) {
        const uint slot = atomicAdd(visibleMeshletCount, 1);
        payload.meshletIndices[slot] = meshletIndex;
    }
    memoryBarrierShared();
    barrier();

    EmitMeshTasksEXT(visibleMeshletCount, 1, 1);
}
