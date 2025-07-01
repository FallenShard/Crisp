#ifndef PATH_TRACER_VERTEX_PULL_PART_GLSL
#define PATH_TRACER_VERTEX_PULL_PART_GLSL

vec3 interpolatePosition(const uvec3 tri, const vec3 bary) {
    return vertices.data[tri[0]] * bary[0]
         + vertices.data[tri[1]] * bary[1]
         + vertices.data[tri[2]] * bary[2];
}

vec3 interpolateNormal(const uvec3 tri, const vec3 bary) {
    return normalize(
          normals.data[tri[0]] * bary[0]
        + normals.data[tri[1]] * bary[1]
        + normals.data[tri[2]] * bary[2]);
}

#endif