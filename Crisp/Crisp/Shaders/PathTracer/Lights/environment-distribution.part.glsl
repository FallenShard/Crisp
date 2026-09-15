#ifndef CRISP_PATH_TRACER_ENVIRONMENT_DISTRIBUTION_GLSL
#define CRISP_PATH_TRACER_ENVIRONMENT_DISTRIBUTION_GLSL

#include "../Core/environment-cdf.part.glsl"

uint environmentColumnCdfOffset(const uint width, const uint height, const uint row) {
    return height + 1u + row * (width + 1u);
}

uint sampleEnvironmentCdf1D(
    const EnvironmentCdf distribution, const uint offset, const uint count, inout float sampleValue) {
    uint lower = 0u;
    uint upper = count;
    while (lower + 1u < upper) {
        const uint middle = (lower + upper) / 2u;
        if (distribution.data[offset + middle] <= sampleValue) {
            lower = middle;
        } else {
            upper = middle;
        }
    }

    const uint index = min(lower, count - 1u);
    const float intervalStart = distribution.data[offset + index];
    const float intervalEnd = distribution.data[offset + index + 1u];
    sampleValue = intervalEnd > intervalStart
        ? clamp((sampleValue - intervalStart) / (intervalEnd - intervalStart), 0.0f, 0.99999994f)
        : 0.5f;
    return index;
}

// Crisp's equirect convention: u = 0.5 looks down -Z, u increases turning right, v = 0 is the zenith, and the
// image is loaded unflipped because v is Vulkan-native. Must stay the inverse of environmentUvToDirection, and
// must match sampleSphericalMap in equirect-to-cube.frag.glsl. See docs/environment-maps.md.
vec2 environmentDirectionToUv(const vec3 direction) {
    const vec3 unitDirection = normalize(direction);
    return vec2(
        fract(atan(unitDirection.x, -unitDirection.z) * InvTwoPI + 0.5f),
        acos(clamp(unitDirection.y, -1.0f, 1.0f)) * InvPI);
}

vec3 environmentUvToDirection(const vec2 uv) {
    const float theta = PI * uv.y;
    const float phi = 2.0f * PI * (uv.x - 0.5f);
    const float sinTheta = sin(theta);
    return vec3(sinTheta * sin(phi), cos(theta), -sinTheta * cos(phi));
}

float environmentDirectionPdf(
    const EnvironmentCdf distribution, const uint width, const uint height, const vec3 direction) {
    const vec2 uv = environmentDirectionToUv(direction);
    const uint x = min(uint(fract(uv.x) * float(width)), width - 1u);
    const uint y = min(uint(clamp(uv.y, 0.0f, 0.99999994f) * float(height)), height - 1u);
    const uint columnOffset = environmentColumnCdfOffset(width, height, y);
    const float rowProbability = distribution.data[y + 1u] - distribution.data[y];
    const float columnProbability =
        distribution.data[columnOffset + x + 1u] - distribution.data[columnOffset + x];
    const float sinTheta = sqrt(max(0.0f, 1.0f - direction.y * direction.y));
    if (sinTheta <= 1e-7f) {
        return 0.0f;
    }
    return rowProbability * columnProbability * float(width * height) / (2.0f * PI * PI * sinTheta);
}

vec3 sampleEnvironmentDirection(
    const EnvironmentCdf distribution,
    const uint width,
    const uint height,
    vec2 sampleValue,
    out vec2 uv,
    out float pdf) {
    const uint row = sampleEnvironmentCdf1D(distribution, 0u, height, sampleValue.y);
    const uint column = sampleEnvironmentCdf1D(
        distribution, environmentColumnCdfOffset(width, height, row), width, sampleValue.x);
    uv = (vec2(column, row) + sampleValue) / vec2(width, height);
    const vec3 direction = environmentUvToDirection(uv);
    pdf = environmentDirectionPdf(distribution, width, height, direction);
    return direction;
}

#endif // CRISP_PATH_TRACER_ENVIRONMENT_DISTRIBUTION_GLSL
