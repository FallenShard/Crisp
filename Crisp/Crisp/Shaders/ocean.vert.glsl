#version 450 core

layout(location = 0) in vec3 position;

layout(set = 0, binding = 0) uniform TransformPack
{
    mat4 MVP;
    mat4 MV;
    mat4 M;
    mat4 N;
};

layout(set = 0, binding = 1) uniform sampler2D displacementMap;
layout(set = 0, binding = 2) uniform sampler2D displacementXMap;
layout(set = 0, binding = 3) uniform sampler2D displacementZMap;
layout(set = 0, binding = 4) uniform sampler2D normalXMap;
layout(set = 0, binding = 5) uniform sampler2D normalZMap;

layout(push_constant) uniform PushConstant
{
    layout(offset = 0) float t;
    layout(offset = 4) uint N;
} pushConst;

layout(location = 0) out vec3 eyePosition;
layout(location = 1) out vec3 eyeNormal;
layout(location = 2) out vec3 worldNormal;

float getFactor(int i, int j) {
    if ((i + j) % 2 == 1)
        return -1;
    else
        return 1;
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

const float size = 50.0;

vec3 makeNormal(int i, int j, float factor) {
    float R = getHeight(i + 1, j, factor);
    float L = getHeight(i - 1, j, factor);
    float T = getHeight(i, j - 1, factor);
    float B = getHeight(i, j + 1, factor);

    float delta = size / pushConst.N;
    vec3 normal = vec3((L - R) / (2 * delta), 1, (B - T) / (2 * delta));
    return normalize(normal);
}

vec3 makeNormal2(int i, int j, float factor) {
    float nx = texelFetch(normalXMap, ivec2(i, j), 0).r * factor * 20;
    float nz = texelFetch(normalZMap, ivec2(i, j), 0).r * factor * 20;
    return normalize(vec3(-nx, 1, -nz));
}

vec3 getDisplacement(int i, int j) {
    float factor = getFactor(i, j);
    float lambda = 0.5;
    return vec3(lambda * getDx(i, j, factor), 0.2 * getHeight(i, j, factor), lambda * getDz(i, j, factor));
}

vec3 makeNormal3(int i, int j, float f) {
    const float delta = size / 2048.0;

    vec3 center = getDisplacement(i, j);
    vec3 right = vec3(+delta, 0, 0) + getDisplacement(i + 1, j) - center;
    vec3 left = vec3(-delta, 0, 0) + getDisplacement(i - 1, j) - center;
    vec3 top = vec3(0, 0, -delta) + getDisplacement(i, j + 1) - center;
    vec3 bottom = vec3(0, 0, +delta) + getDisplacement(i, j - 1) - center;
    
    vec3 rt = cross(right, top);
    vec3 tl = cross(top, left);
    vec3 lb = cross(left, bottom);
    vec3 br = cross(bottom, right);

    return normalize(rt + tl + lb + br);//vec3(center, 0.0, 0.0);
    //return vec3(0, 1, 0);
    //return normalize(rt + tl + lb + br);
}

void main()
{
    int col = int(gl_VertexIndex % pushConst.N);
    int row = int(gl_VertexIndex / pushConst.N);
    float factor = getFactor(col, row);
    float height = getHeight(col, row, factor);

    vec3 pos = position + getDisplacement(col, row);
    worldNormal = makeNormal3(col, row, factor);

    eyePosition = (MV * vec4(pos, 1.0f)).xyz;
    eyeNormal = (N * vec4(worldNormal, 0.0f)).xyz;
    //float texVal = texture(displacementMap, texCoord).x;
    //pos.y = 0.2 * sin(pushConst.t + 2 * pos.x);
    //pos.y += 0.1 * sin(pushConst.t + 10 * pos.x);
    //pos.y += 0.4 * sin(pushConst.t + 5 * pos.x);
    gl_Position  = MVP * vec4(pos, 1.0f);
}