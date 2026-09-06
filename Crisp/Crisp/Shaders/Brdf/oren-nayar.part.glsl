#ifndef CRISP_OREN_NAYAR_GLSL
#define CRISP_OREN_NAYAR_GLSL

#include "../Common/math-constants.part.glsl"

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

    return reflectance * InvPI * evaluateOrenNayarRoughness(wi, wo, roughness) * wo.z;
}

// EON, Portsmouth, Kutz and Hill, JCGT 14(1) 2025: https://jcgt.org/published/0014/01/06/ -- Listing 1.
// Roughness is the paper's r in [0, 1], as OpenPBR's base_diffuse_roughness is; QON above reads its as sigma.
// OpenPBR mandates this lobe. QON stays for the legacy oren-nayar material, its only remaining caller.

// 1/2 - 2/(3 pi) and 2/3 - 28/(15 pi), the two quadrature constants the FON albedo closes to.
const float kFonConstant1 = 0.5f - 2.0f / (3.0f * PI);
const float kFonConstant2 = 2.0f / 3.0f - 28.0f / (15.0f * PI);

float fonA(const float roughness) {
    return 1.0f / (1.0f + kFonConstant1 * roughness);
}

// The paper's exact E_FON. Its quartic fit avoids the acos; this is a reference path, so swap only on a profile.
float fonDirectionalAlbedo(const float cosTheta, const float roughness) {
    const float mu = clamp(cosTheta, 1e-6f, 1.0f);
    const float sinTheta = sqrt(max(0.0f, 1.0f - mu * mu));
    const float g =
        sinTheta * (acos(mu) - sinTheta * mu) +
        (2.0f / 3.0f) * ((sinTheta / mu) * (1.0f - sinTheta * sinTheta * sinTheta) - sinTheta);
    const float a = fonA(roughness);
    return a + (roughness * a) * InvPI * g;
}

// Closed form rather than a second fit: the compensation conserves energy only when this is genuinely the
// average of the E beside it.
float fonAverageAlbedo(const float roughness) {
    return fonA(roughness) * (1.0f + kFonConstant2 * roughness);
}

// Differs from QON only in the t term -- QON drops to zero when s <= 0, FON keeps t = 1, which is what lets the
// albedo integrate in closed form.
vec3 evaluateFonSingleScatter(const vec3 reflectance, const float roughness, const vec3 wi, const vec3 wo) {
    const float s = dot(wi, wo) - wi.z * wo.z;
    const float sOverT = s > 0.0f ? s / max(wi.z, wo.z) : s;
    return reflectance * InvPI * fonA(roughness) * (1.0f + roughness * sOverT);
}

// Returns f * cos(theta_o) to match evaluateOrenNayar. The caller folds any diffuse weight into reflectance,
// as OpenPBR's w_d does.
vec3 evaluateEon(const vec3 reflectance, const float roughness, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    const float averageAlbedo = fonAverageAlbedo(roughness);
    const vec3 multipleScatterAlbedo =
        reflectance * reflectance * averageAlbedo / max(vec3(1e-7f), 1.0f - reflectance * (1.0f - averageAlbedo));

    // The epsilons guard roughness 0, where all three deficits vanish together and the ratio is 0/0.
    const vec3 compensation = multipleScatterAlbedo * InvPI *
        max(1e-7f, 1.0f - fonDirectionalAlbedo(wo.z, roughness)) *
        max(1e-7f, 1.0f - fonDirectionalAlbedo(wi.z, roughness)) / max(1e-7f, 1.0f - averageAlbedo);

    return (evaluateFonSingleScatter(reflectance, roughness, wi, wo) + compensation) * wo.z;
}

#endif // CRISP_OREN_NAYAR_GLSL
