#ifndef CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL
#define CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL

// Smooth value noise in [0, 1]. The final smoke density also stays in [0, 1],
// so the largest RGB extinction coefficient remains a valid Woodcock majorant.
float mediumNoiseHash(const ivec3 cell) {
    const uvec3 h = pcg3d(uvec3(cell));
    return uintToUnitFloat(h.x);
}

float mediumValueNoise(const vec3 coordinate) {
    const ivec3 cell = ivec3(floor(coordinate));
    const vec3 f = fract(coordinate);
    const vec3 u = f * f * (3.0f - 2.0f * f);
    const float x00 = mix(mediumNoiseHash(cell), mediumNoiseHash(cell + ivec3(1, 0, 0)), u.x);
    const float x10 = mix(mediumNoiseHash(cell + ivec3(0, 1, 0)), mediumNoiseHash(cell + ivec3(1, 1, 0)), u.x);
    const float x01 = mix(mediumNoiseHash(cell + ivec3(0, 0, 1)), mediumNoiseHash(cell + ivec3(1, 0, 1)), u.x);
    const float x11 = mix(mediumNoiseHash(cell + ivec3(0, 1, 1)), mediumNoiseHash(cell + ivec3(1, 1, 1)), u.x);
    return mix(mix(x00, x10, u.y), mix(x01, x11, u.y), u.z);
}

// A rising plume: widening, gently drifting cross-section with noisy billows.
// Coordinates are normalized by the scene's AABB, so moving or resizing the
// volume does not change its silhouette. Both edges and ends fade to vacuum.
float heterogeneousMediumDensity(
    const vec3 position, const float noiseScale, const vec3 boundsMin, const vec3 boundsMax) {
    const vec3 p = clamp((position - boundsMin) / (boundsMax - boundsMin), vec3(0.0f), vec3(1.0f));
    const vec2 center = vec2(
        0.5f + 0.11f * sin(5.0f * p.y + 0.7f),
        0.5f + 0.08f * cos(4.0f * p.y + 0.2f));
    const vec2 radius = mix(vec2(0.17f, 0.2f), vec2(0.38f, 0.42f), p.y);
    const vec3 coordinate = p * noiseScale;
    const float large = mediumValueNoise(coordinate + vec3(3.7f, 9.1f, 1.3f));
    const float middle = mediumValueNoise(2.07f * coordinate + vec3(13.1f, 2.7f, 7.9f));
    const float fine = mediumValueNoise(4.13f * coordinate + vec3(5.3f, 17.1f, 11.7f));
    const float radial = length((p.xz - center) / radius);
    const float edge = 1.0f - smoothstep(0.65f, 1.15f, radial + 0.3f * (0.5f - middle));
    const float vertical = smoothstep(0.0f, 0.12f, p.y) * (1.0f - smoothstep(0.82f, 1.0f, p.y));
    const float billows = smoothstep(0.2f, 0.8f, 0.55f * large + 0.3f * middle + 0.15f * fine);
    return clamp(edge * vertical * (0.3f + 0.7f * billows), 0.0f, 1.0f);
}

float mediumMajorant(const vec3 extinction) {
    return max(extinction.x, max(extinction.y, extinction.z));
}

// An independent stream for each variable-length tracking walk. This keeps
// null collisions from consuming the fixed BSDF/light/phase dimensions.
Sampler createMediumTrackingSampler(const Sampler pathSampler, const uint bounceDim, const uint stream) {
    Sampler trackingSampler;
    trackingSampler.seed = pcg3d(uvec3(pathSampler.seed, pathSampler.sampleIdx, bounceDim ^ stream)).x;
    trackingSampler.sampleIdx = pathSampler.sampleIdx;
    trackingSampler.dimension = 0u;
    return trackingSampler;
}

// Scalar proposal accepts at max(R,G,B) local extinction. At each null event,
// the RGB likelihood ratio preserves unbiased per-channel transport; a real
// collision contributes the local RGB scattering / scalar proposal density.
float sampleHeterogeneousMediumDistance(
    inout Sampler rng, const vec3 origin, const vec3 direction, const float maxDistance,
    const vec3 extinction, const vec3 scattering, const float noiseScale,
    const vec3 boundsMin, const vec3 boundsMax, inout vec3 weight) {
    const float majorant = mediumMajorant(extinction);
    if (majorant <= 0.0f) {
        return maxDistance;
    }

    float distance = 0.0f;
    while (true) {
        distance += -log(max(1.0f - next1D(rng), 1e-7f)) / majorant;
        if (distance >= maxDistance) {
            return maxDistance;
        }

        const float density = heterogeneousMediumDensity(origin + distance * direction, noiseScale, boundsMin, boundsMax);
        const vec3 localExtinction = extinction * density;
        const float localMaximum = mediumMajorant(localExtinction);
        if (localMaximum <= 0.0f) {
            continue;
        }
        const float acceptProbability = localMaximum / majorant;
        if (next1D(rng) < acceptProbability) {
            weight *= scattering * density / localMaximum;
            return distance;
        }
        weight *= (vec3(1.0f) - localExtinction / majorant) / (1.0f - acceptProbability);
    }
}

// Ratio tracking estimates RGB transmittance along the unoccluded shadow ray.
vec3 heterogeneousMediumTransmittance(
    inout Sampler rng, const vec3 origin, const vec3 direction, const float maxDistance,
    const vec3 extinction, const float noiseScale, const vec3 boundsMin, const vec3 boundsMax) {
    const float majorant = mediumMajorant(extinction);
    if (majorant <= 0.0f) {
        return vec3(1.0f);
    }

    vec3 transmittance = vec3(1.0f);
    float distance = 0.0f;
    while (true) {
        distance += -log(max(1.0f - next1D(rng), 1e-7f)) / majorant;
        if (distance >= maxDistance) {
            return transmittance;
        }
        const float density = heterogeneousMediumDensity(origin + distance * direction, noiseScale, boundsMin, boundsMax);
        transmittance *= vec3(1.0f) - extinction * (density / majorant);
    }
}

#endif // CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL
