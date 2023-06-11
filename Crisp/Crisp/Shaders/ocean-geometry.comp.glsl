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

const float g = 9.81;

const float signValue[2] = {1, -1};
float getFactor(int i, int j) {
    return signValue[1 - (i + j) % 2];
}

const float texelCenterOffset = 0.5;

float getHeight(int i, int j, float factor) {
    return texelFetch(displacementMap, ivec2(i, j), 0).r * factor;
}

float getDx(int i, int j, float factor) {
    return texelFetch(displacementXMap, ivec2(i, j), 0).r * factor;
}

float getDz(int i, int j, float factor) {
    return texelFetch(displacementZMap, ivec2(i, j), 0).r * factor;
}

vec3 makeNormal2(int i, int j, float factor) {
    float nx = texelFetch(normalXMap, ivec2(i, j), 0).r * factor * patchWorldSize;
    float nz = texelFetch(normalZMap, ivec2(i, j), 0).r * factor * patchWorldSize;
    return normalize(vec3(-nx, 1, -nz));
}

vec3 getDisplacement(int i, int j) {
    float factor = getFactor(i, j);
    float chop = -choppiness;
    float x = chop * getDx(i, j, factor);
    float y = getHeight(i, j, factor);
    float z = chop * getDz(i, j, factor);
    return vec3(x, y, z);
}

// Cannot apply standard heightmap -> normal map computation because there are
// displacements in X and Z as well.
vec3 makeNormal3(int i, int j, float f) {
    const float cellSize = patchWorldSize / N;

    const vec3 center = getDisplacement(i, j);
    const vec3 right = vec3(+cellSize, 0, 0) + getDisplacement((i + 1) % N, j) - center;
    const vec3 left = vec3(-cellSize, 0, 0) + getDisplacement((i + N - 1) % N, j) - center;
    const vec3 top = vec3(0, 0, -cellSize) + getDisplacement(i, (j + 1) % N) - center;
    const vec3 bottom = vec3(0, 0, +cellSize) + getDisplacement(i, (j + N - 1) % N) - center;
    
    const vec3 rt = cross(right, top);
    const vec3 tl = cross(top, left);
    const vec3 lb = cross(left, bottom);
    const vec3 br = cross(bottom, right);

    return normalize(rt + tl + lb + br);
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

    const int col = idx.x % N;
    const int row = idx.y % N;
    const float factor = getFactor(col, row);

    const vec3 startPos = vec3(idx.x, 0.0, idx.y) * cellSize - vec3(patchWorldSize * 0.5, 0, patchWorldSize * 0.5);
    const vec3 pos = startPos + getDisplacement(col, row);
    
    const int linIdx = idx.y * (N + 1) + idx.x;
    writePosition(linIdx, pos);

    vec3 startNormal = vec3(0, 1, 0);//makeNormal3(col, row, factor);
    startNormal = makeNormal3(col, row, factor);
    writeNormal(linIdx, startNormal);
}