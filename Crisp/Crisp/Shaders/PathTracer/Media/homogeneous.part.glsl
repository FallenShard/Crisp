#ifndef CRISP_PATH_TRACER_HOMOGENEOUS_MEDIUM_GLSL
#define CRISP_PATH_TRACER_HOMOGENEOUS_MEDIUM_GLSL

float homogeneousMediumTransmittance(const float extinction, const float distance) {
    return exp(-max(extinction, 0.0f) * max(distance, 0.0f));
}

vec3 homogeneousMediumTransmittance(const vec3 extinction, const float distance) {
    return exp(-max(extinction, vec3(0.0f)) * max(distance, 0.0f));
}

float sampleHomogeneousMediumDistance(const float unitSample, const float extinction) {
    if (extinction <= 0.0f) {
        return 1e30f;
    }
    return -log(max(1.0f - unitSample, 1e-7f)) / extinction;
}

// Sample one RGB extinction channel uniformly; the other two enter the mixture PDF below.
float sampleHomogeneousMediumDistance(const vec2 unitSample, const vec3 extinction) {
    const int channel = min(int(unitSample.x * 3.0f), 2);
    return sampleHomogeneousMediumDistance(unitSample.y, extinction[channel]);
}

float homogeneousMediumDistancePdf(const vec3 extinction, const vec3 transmittance, const bool mediumEvent) {
    const vec3 density = mediumEvent ? extinction * transmittance : transmittance;
    return (density.x + density.y + density.z) / 3.0f;
}

#endif // CRISP_PATH_TRACER_HOMOGENEOUS_MEDIUM_GLSL
