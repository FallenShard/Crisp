#ifndef CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL
#define CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL

float mediumMajorant(const vec3 extinction) {
    return max(extinction.x, max(extinction.y, extinction.z));
}

// An independent stream for each variable-length tracking walk.
Sampler createMediumTrackingSampler(const Sampler pathSampler, const uint bounceDim, const uint stream) {
    Sampler trackingSampler;
    trackingSampler.seed = pcg3d(uvec3(pathSampler.seed, pathSampler.sampleIdx, bounceDim ^ stream)).x;
    trackingSampler.sampleIdx = pathSampler.sampleIdx;
    trackingSampler.dimension = 0u;
    return trackingSampler;
}

float heterogeneousMediumDensity(const vec3 position, const vec3 boundsMin, const vec3 boundsMax) {
    const vec3 uvw = (position - boundsMin) / (boundsMax - boundsMin);
    return texture(mediumVolume, uvw).r;
}

float sampleHeterogeneousMediumDistance(
    inout Sampler rng, const vec3 origin, const vec3 direction, const float maxDistance,
    const vec3 extinction, const vec3 scattering, const float maximumDensity,
    const vec3 boundsMin, const vec3 boundsMax, inout vec3 weight) {
    const float majorant = mediumMajorant(extinction) * maximumDensity;
    if (majorant <= 0.0f) {
        return maxDistance;
    }

    float distance = 0.0f;
    while (true) {
        distance += -log(max(1.0f - next1D(rng), 1e-7f)) / majorant;
        if (distance >= maxDistance) {
            return maxDistance;
        }

        const float density = heterogeneousMediumDensity(origin + distance * direction, boundsMin, boundsMax);
        const vec3 localExtinction = extinction * density;
        const float localMaximum = mediumMajorant(localExtinction);
        if (localMaximum <= 0.0f) {
            continue;
        }
        const float acceptProbability = min(localMaximum / majorant, 1.0f);
        if (next1D(rng) < acceptProbability) {
            weight *= scattering * density / localMaximum;
            return distance;
        }
        weight *= (vec3(1.0f) - localExtinction / majorant) / (1.0f - acceptProbability);
    }
}

vec3 heterogeneousMediumTransmittance(
    inout Sampler rng, const vec3 origin, const vec3 direction, const float maxDistance,
    const vec3 extinction, const float maximumDensity,
    const vec3 boundsMin, const vec3 boundsMax) {
    const float majorant = mediumMajorant(extinction) * maximumDensity;
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
        const float density = heterogeneousMediumDensity(origin + distance * direction, boundsMin, boundsMax);
        transmittance *= vec3(1.0f) - extinction * (density / majorant);
    }
}

#endif // CRISP_PATH_TRACER_HETEROGENEOUS_MEDIUM_GLSL
