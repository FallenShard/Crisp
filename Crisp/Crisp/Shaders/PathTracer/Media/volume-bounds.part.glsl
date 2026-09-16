#ifndef CRISP_PATH_TRACER_VOLUME_BOUNDS_GLSL
#define CRISP_PATH_TRACER_VOLUME_BOUNDS_GLSL

// Intersect a finite ray segment with a world-space medium box. Zero direction
// components are handled explicitly, avoiding NaNs from 0 * infinity at faces.
bool intersectMediumBounds(
    const vec3 origin, const vec3 direction, const float maxDistance,
    const vec3 boundsMin, const vec3 boundsMax, out float entry, out float exitDistance) {
    entry = 0.0f;
    exitDistance = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (abs(direction[axis]) < 1e-8f) {
            if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis]) {
                return false;
            }
            continue;
        }
        const float inverseDirection = 1.0f / direction[axis];
        const float nearDistance = (boundsMin[axis] - origin[axis]) * inverseDirection;
        const float farDistance = (boundsMax[axis] - origin[axis]) * inverseDirection;
        entry = max(entry, min(nearDistance, farDistance));
        exitDistance = min(exitDistance, max(nearDistance, farDistance));
        if (entry >= exitDistance) {
            return false;
        }
    }
    return entry < exitDistance;
}

#endif // CRISP_PATH_TRACER_VOLUME_BOUNDS_GLSL
