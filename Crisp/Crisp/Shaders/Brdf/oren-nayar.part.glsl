#ifndef CRISP_OREN_NAYAR_GLSL
#define CRISP_OREN_NAYAR_GLSL

float evaluateOrenNayarRoughness(vec3 wi, vec3 wo, float roughness) {
    const float cosThetaI = wi.z;
    const float cosThetaO = wo.z;
    const float sinThetaI = sqrt(max(0.0f, 1.0f - cosThetaI * cosThetaI));
    const float sinThetaO = sqrt(max(0.0f, 1.0f - cosThetaO * cosThetaO));

    float maxCosPhiDifference = 0.0f;
    if (sinThetaI > 0.0f && sinThetaO > 0.0f) {
        maxCosPhiDifference = clamp(dot(wi.xy, wo.xy) / (sinThetaI * sinThetaO), 0.0f, 1.0f);
    }

    float sinAlpha;
    float tanBeta;
    if (cosThetaI > cosThetaO) {
        sinAlpha = sinThetaO;
        tanBeta = sinThetaI / cosThetaI;
    } else {
        sinAlpha = sinThetaI;
        tanBeta = sinThetaO / cosThetaO;
    }

    const float roughnessSquared = roughness * roughness;
    const float a = 1.0f - roughnessSquared / (2.0f * (roughnessSquared + 0.33f));
    const float b = 0.45f * roughnessSquared / (roughnessSquared + 0.09f);
    return a + b * maxCosPhiDifference * sinAlpha * tanBeta;
}

vec3 evaluateOrenNayar(vec3 reflectance, float roughness, vec3 wi, vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    return reflectance / PI * evaluateOrenNayarRoughness(wi, wo, roughness) * wo.z;
}

#endif // CRISP_OREN_NAYAR_GLSL
