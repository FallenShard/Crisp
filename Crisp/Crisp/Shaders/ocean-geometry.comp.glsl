#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0) buffer Positions {
    float positions[];
};

// Normals come from the spectral slopes in ocean.frag.glsl; differences of this grid are not exact.
layout(set = 0, binding = 1) uniform sampler2D packedHeightDispXMap;
layout(set = 0, binding = 2) uniform sampler2D packedDispZNormalXMap;

layout(push_constant) uniform PushConstant {
    int N;
    float patchWorldSize;
    float choppiness;
};

// Tessendorf choppy waves; the spectrum's D(k) = -i*(k/|k|)*h~ convention makes the scale negative.
vec3 getDisplacement(int i, int j) {
    const float height = texelFetch(packedHeightDispXMap, ivec2(i, j), 0).r;
    const float dispX = texelFetch(packedHeightDispXMap, ivec2(i, j), 0).g;
    const float dispZ = texelFetch(packedDispZNormalXMap, ivec2(i, j), 0).r;
    return vec3(-choppiness * dispX, height, -choppiness * dispZ);
}

void main() {
    const ivec2 idx = ivec2(gl_GlobalInvocationID.xy);
    if (idx.x > N || idx.y > N) {
        return;
    }

    const float cellSize = patchWorldSize / N;

    // Last row/column wraps onto the first so neighbouring patches tile seamlessly.
    const int col = idx.x % N;
    const int row = idx.y % N;

    // Matches createGridMesh: i runs +X, j runs +Z.
    const vec3 startPos = vec3(idx.x, 0.0, idx.y) * cellSize - vec3(patchWorldSize * 0.5, 0, patchWorldSize * 0.5);
    const vec3 newPos = startPos + getDisplacement(col, row);

    const int linIdx = idx.y * (N + 1) + idx.x;
    positions[3 * linIdx + 0] = newPos[0];
    positions[3 * linIdx + 1] = newPos[1];
    positions[3 * linIdx + 2] = newPos[2];
}
