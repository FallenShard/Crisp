#ifndef PATH_TRACER_VERTEX_PULL_PART_GLSL
#define PATH_TRACER_VERTEX_PULL_PART_GLSL

const int kVertexComponentCount = 6;
const int kPositionComponentOffset = 0;
const int kNormalComponentOffset = 3;

vec3 interpolatePosition(const uvec3 tri, const vec3 bary) {
    mat3 positions = mat3(
        vec3(
            vertices.data[kVertexComponentCount * tri[0] + kPositionComponentOffset],
            vertices.data[kVertexComponentCount * tri[0] + kPositionComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[0] + kPositionComponentOffset + 2]),
        vec3(
            vertices.data[kVertexComponentCount * tri[1] + kPositionComponentOffset],
            vertices.data[kVertexComponentCount * tri[1] + kPositionComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[1] + kPositionComponentOffset + 2]),
        vec3(
            vertices.data[kVertexComponentCount * tri[2] + kPositionComponentOffset],
            vertices.data[kVertexComponentCount * tri[2] + kPositionComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[2] + kPositionComponentOffset + 2])
    );
    return positions * bary;
}

vec3 interpolateNormal(const uvec3 tri, const vec3 bary) {
    mat3 normals = mat3(
        vec3(
            vertices.data[kVertexComponentCount * tri[0] + kNormalComponentOffset],
            vertices.data[kVertexComponentCount * tri[0] + kNormalComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[0] + kNormalComponentOffset + 2]),
        vec3(
            vertices.data[kVertexComponentCount * tri[1] + kNormalComponentOffset],
            vertices.data[kVertexComponentCount * tri[1] + kNormalComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[1] + kNormalComponentOffset + 2]),
        vec3(
            vertices.data[kVertexComponentCount * tri[2] + kNormalComponentOffset],
            vertices.data[kVertexComponentCount * tri[2] + kNormalComponentOffset + 1],
            vertices.data[kVertexComponentCount * tri[2] + kNormalComponentOffset + 2])
    );
    return normalize(normals * bary);
}

#endif