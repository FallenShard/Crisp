#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(set = 0, binding = 0) buffer Positions
{
    float positions[];
};

layout (set = 0, binding = 1) buffer Normals
{
    float normals[];
};

layout(set = 0, binding = 2) uniform sampler2D displacementMap;
layout(set = 0, binding = 3) uniform sampler2D displacementXMap;
layout(set = 0, binding = 4) uniform sampler2D displacementZMap;
layout(set = 0, binding = 5) uniform sampler2D normalXMap;
layout(set = 0, binding = 6) uniform sampler2D normalZMap;

layout(push_constant) uniform PushConstant
{
    int N;
    float patchWorldSize;
    float choppiness;
};

// Undoes the spectral origin shift: k_m = 2*pi*(m - N/2) / L costs a (-1)^n vs a plain IDFT.
float getFactor(int i, int j) {
    return ((i + j) & 1) == 0 ? 1.0 : -1.0;
}

float getHeight(int i, int j, float factor) {
    return texelFetch(displacementMap, ivec2(i, j), 0).r * factor;
}

float getDx(int i, int j, float factor) {
    return texelFetch(displacementXMap, ivec2(i, j), 0).r * factor;
}

float getDz(int i, int j, float factor) {
    return texelFetch(displacementZMap, ivec2(i, j), 0).r * factor;
}

// Choppy waves (Tessendorf, Simulating Ocean Water, "Choppy Waves"). The spectrum pass uses the
// D(k) = -i * (k / |k|) * h~(k) convention, which requires a negative scale: x' = x - lambda * D.
// A positive scale converges on troughs instead of crests. All components share one sign factor.
vec3 getDisplacement(int i, int j) {
    const float factor = getFactor(i, j);
    return vec3(-choppiness * getDx(i, j, factor),
                getHeight(i, j, factor),
                -choppiness * getDz(i, j, factor));
}

// Not a plain heightmap gradient: there is displacement in X and Z too. Index i runs +X, j runs
// +Z; the cross products are ordered to give +Y for a flat patch.
vec3 makeNormal(int i, int j) {
    const float cellSize = patchWorldSize / N;

    const vec3 center = getDisplacement(i, j);
    const vec3 right = vec3(+cellSize, 0, 0) + getDisplacement((i + 1) % N, j) - center;
    const vec3 left = vec3(-cellSize, 0, 0) + getDisplacement((i + N - 1) % N, j) - center;
    const vec3 front = vec3(0, 0, +cellSize) + getDisplacement(i, (j + 1) % N) - center;
    const vec3 back = vec3(0, 0, -cellSize) + getDisplacement(i, (j + N - 1) % N) - center;

    return normalize(cross(front, right) + cross(right, back) + cross(back, left) + cross(left, front));
}

vec3 readPosition(int linIdx) {
    return vec3(positions[3 * linIdx + 0], positions[3 * linIdx + 1], positions[3 * linIdx + 2]);
}

void writePosition(int linIdx, vec3 newPos) {
    positions[3 * linIdx + 0] = newPos[0];
    positions[3 * linIdx + 1] = newPos[1];
    positions[3 * linIdx + 2] = newPos[2];
}

void writeNormal(int linIdx, vec3 newNormal) {
    normals[3 * linIdx + 0] = newNormal[0];
    normals[3 * linIdx + 1] = newNormal[1];
    normals[3 * linIdx + 2] = newNormal[2];
}

void main()
{
    // There are N + 1 vertices in X and Z axis.
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

    const int linIdx = idx.y * (N + 1) + idx.x;
    writePosition(linIdx, startPos + getDisplacement(col, row));
    writeNormal(linIdx, makeNormal(col, row));
}