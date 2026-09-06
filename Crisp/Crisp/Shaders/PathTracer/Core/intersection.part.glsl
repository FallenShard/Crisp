#ifndef CRISP_PATH_TRACER_INTERSECTION_GLSL
#define CRISP_PATH_TRACER_INTERSECTION_GLSL

vec3 interpolatePosition(PathTraceVertices positions, const uvec3 tri, const vec3 bary) {
    return positions.data[tri[0]] * bary[0] + positions.data[tri[1]] * bary[1] +
           positions.data[tri[2]] * bary[2];
}

vec3 interpolateNormal(PathTraceNormals normals, const uvec3 tri, const vec3 bary) {
    return normalize(
        normals.data[tri[0]] * bary[0] + normals.data[tri[1]] * bary[1] + normals.data[tri[2]] * bary[2]);
}

vec2 interpolateTexCoord(PathTraceTexCoords texCoords, const uvec3 tri, const vec3 bary) {
    return texCoords.data[tri[0]] * bary[0] + texCoords.data[tri[1]] * bary[1] +
           texCoords.data[tri[2]] * bary[2];
}

#endif // CRISP_PATH_TRACER_INTERSECTION_GLSL
