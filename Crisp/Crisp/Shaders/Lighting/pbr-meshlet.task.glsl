#version 460 core
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/view.part.glsl"
#include "../Geometry/meshlet-culling.part.glsl"

layout(local_size_x = kTaskWorkGroupSize) in;

layout(set = 1, binding = 0) uniform View {
    ViewParameters view;
};

layout(std430, set = 3, binding = 3) readonly buffer MeshletBoundsBuffer {
    MeshletBounds meshletBounds[];
};

#include "pbr-meshlet-draw.part.glsl"

taskPayloadSharedEXT MeshletTaskPayload payload;

shared uint visibleWriteIndex;

void main() {
    if (gl_LocalInvocationIndex == 0) {
        visibleWriteIndex = 0;
    }
    barrier();

    const uint localIndex = gl_GlobalInvocationID.x;
    const uint meshletIndex = meshletDraw.firstMeshlet + localIndex;
    const bool withinRange = localIndex < meshletDraw.meshletCount;

    const vec3 cameraPosition = view.invV[3].xyz;
    const bool visible = withinRange &&
        (meshletDraw.cullingEnabled == 0 || isMeshletConeVisible(meshletBounds[meshletIndex], cameraPosition));

    if (visible) {
        const uint slot = atomicAdd(visibleWriteIndex, 1);
        payload.meshletIndices[slot] = meshletIndex;
    }
    memoryBarrierShared();
    barrier();

    EmitMeshTasksEXT(visibleWriteIndex, 1, 1);
}
