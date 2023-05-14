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
    float choppiness;
};

const float g = 9.81;

const float signValue[2] = {1, -1};
float getFactor(int i, int j) {
    return signValue[1 - (i + j) % 2];
}

const float texelCenterOffset = 0.0;

float getHeight(int i, int j, float factor) {
    // return texelFetch(displacementMap, ivec2(i, j), 0).r * factor;
    float u = (i + texelCenterOffset) / N;
    float v = (j + texelCenterOffset) / N;
    return texture(displacementMap, vec2(u, v)).r * factor;
}

float getDx(int i, int j, float factor) {
    //return texelFetch(displacementXMap, ivec2(i, j), 0).r * factor;

    float u = (i + texelCenterOffset) / N;
    float v = (j + texelCenterOffset) / N;
    return texture(displacementXMap, vec2(u, v)).r * factor;
}

float getDz(int i, int j, float factor) {
    //return texelFetch(displacementZMap, ivec2(i, j), 0).r * factor;

    float u = (i + texelCenterOffset) / N;
    float v = (j + texelCenterOffset) / N;
    return texture(displacementZMap, vec2(u, v)).r * factor;
}

const float size = 25.0f;

vec3 makeNormal2(int i, int j, float factor) {
    float nx = texelFetch(normalXMap, ivec2(i, j), 0).r * factor * size * 2;
    float nz = texelFetch(normalZMap, ivec2(i, j), 0).r * factor * size * 2;
    return normalize(vec3(-nx, 1, -nz));
}

vec3 getDisplacement(int i, int j) {
    float factor = getFactor(i, j);
    return vec3(-choppiness * getDx(i, j, factor), getHeight(i, j, factor), -choppiness * getDz(i, j, factor));
}

// Cannot apply standard heightmap -> normal map computation because there are
// displacements in X and Z as well.
vec3 makeNormal3(int i, int j, float f) {
    const float delta = size / N;

    const vec3 center = getDisplacement(i, j);
    const vec3 right = vec3(+delta, 0, 0) + getDisplacement(i + 1, j) - center;
    const vec3 left = vec3(-delta, 0, 0) + getDisplacement(i - 1, j) - center;
    const vec3 top = vec3(0, 0, -delta) + getDisplacement(i, j + 1) - center;
    const vec3 bottom = vec3(0, 0, +delta) + getDisplacement(i, j - 1) - center;
    
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
    const ivec2 idx = ivec2(gl_GlobalInvocationID.xy);
    const int linIdx = idx.y * N + idx.x;
    const int col = idx.x;
    const int row = idx.y;
    const float factor = getFactor(col, row);
    const float height = getHeight(col, row, factor);

    vec3 startPos = vec3(col, 0.0, row) / (N - 1) * size - vec3(size * 0.5, 0, size * 0.5);
    vec3 pos = startPos + getDisplacement(col, row);
    writePosition(linIdx, pos);

    vec3 startNormal = vec3(0, 1, 0);//makeNormal3(col, row, factor);
    startNormal = makeNormal2(col, row, factor);
    writeNormal(linIdx, startNormal);
}