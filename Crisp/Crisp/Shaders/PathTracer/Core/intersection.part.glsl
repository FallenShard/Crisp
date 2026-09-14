#ifndef CRISP_PATH_TRACER_INTERSECTION_GLSL
#define CRISP_PATH_TRACER_INTERSECTION_GLSL

#include "instance.part.glsl"

vec3 interpolatePosition(PathTracedPositions positions, const uvec3 tri, const vec3 bary) {
    return positions.data[tri[0]] * bary[0] + positions.data[tri[1]] * bary[1] +
           positions.data[tri[2]] * bary[2];
}

vec3 interpolateNormal(PathTracedAttributes attributes, const uvec3 tri, const vec3 bary) {
    return normalize(
        attributes.data[tri[0]].normal * bary[0] + attributes.data[tri[1]].normal * bary[1] +
        attributes.data[tri[2]].normal * bary[2]);
}

vec2 interpolateTexCoord(PathTracedAttributes attributes, const uvec3 tri, const vec3 bary) {
    return attributes.data[tri[0]].texCoord * bary[0] + attributes.data[tri[1]].texCoord * bary[1] +
           attributes.data[tri[2]].texCoord * bary[2];
}

// Left unnormalized: the caller orthogonalises against the shading normal before using it.
vec4 interpolateTangent(PathTracedAttributes attributes, const uvec3 tri, const vec3 bary) {
    return attributes.data[tri[0]].tangent * bary[0] + attributes.data[tri[1]].tangent * bary[1] +
           attributes.data[tri[2]].tangent * bary[2];
}

#endif // CRISP_PATH_TRACER_INTERSECTION_GLSL
