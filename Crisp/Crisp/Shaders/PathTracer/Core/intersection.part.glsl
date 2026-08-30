#ifndef CRISP_PATH_TRACER_INTERSECTION_GLSL
#define CRISP_PATH_TRACER_INTERSECTION_GLSL

vec3 interpolatePosition(const uvec3 tri, const vec3 bary) {
    return scene.vertices.data[tri[0]] * bary[0] + scene.vertices.data[tri[1]] * bary[1] +
           scene.vertices.data[tri[2]] * bary[2];
}

vec3 interpolateNormal(const uvec3 tri, const vec3 bary) {
    return normalize(
        scene.normals.data[tri[0]] * bary[0] + scene.normals.data[tri[1]] * bary[1] +
        scene.normals.data[tri[2]] * bary[2]);
}

vec2 interpolateTexCoord(const uvec3 tri, const vec3 bary) {
    return scene.texCoords.data[tri[0]] * bary[0] + scene.texCoords.data[tri[1]] * bary[1] +
           scene.texCoords.data[tri[2]] * bary[2];
}

#endif // CRISP_PATH_TRACER_INTERSECTION_GLSL
