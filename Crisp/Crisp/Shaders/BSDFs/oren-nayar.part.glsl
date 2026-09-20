#ifndef CRISP_OREN_NAYAR_GLSL
#define CRISP_OREN_NAYAR_GLSL

#include "../Common/math-constants.part.glsl"

// The energy-preserving Oren-Nayar of Portsmouth, Kutz and Hill, JCGT 14(1) 2025,
// https://jcgt.org/published/0014/01/06/, Listing 1 -- which is the lobe used in OpenPBR 1.1.

// 1/2 - 2/(3 pi) and 2/3 - 28/(15 pi), the two quadrature constants the FON albedo closes to.
const float kOrenNayarConstant1 = 0.5f - 2.0f / (3.0f * PI);
const float kOrenNayarConstant2 = 2.0f / 3.0f - 28.0f / (15.0f * PI);

float orenNayarA(const float roughness) {
    return 1.0f / (1.0f + kOrenNayarConstant1 * roughness);
}

// The paper's exact E_FON. Its quartic fit avoids the acos; this is a reference path, so swap only on a profile.
float orenNayarDirectionalAlbedo(const float cosTheta, const float roughness) {
    const float mu = clamp(cosTheta, 1e-6f, 1.0f);
    const float sinTheta = sqrt(max(0.0f, 1.0f - mu * mu));
    const float g =
        sinTheta * (acos(mu) - sinTheta * mu) +
        (2.0f / 3.0f) * ((sinTheta / mu) * (1.0f - sinTheta * sinTheta * sinTheta) - sinTheta);
    const float a = orenNayarA(roughness);
    return a + (roughness * a) * InvPI * g;
}

// Closed form rather than a second fit: the compensation conserves energy only when this is genuinely the
// average of the E beside it.
float orenNayarAverageAlbedo(const float roughness) {
    return orenNayarA(roughness) * (1.0f + kOrenNayarConstant2 * roughness);
}

// Everything the model needs is the two cosines and the angle between the directions, so this form serves the
// rasteriser -- which has N.L, N.V and L.V but no tangent frame -- as directly as it serves the path tracer.
//
// Returns f alone. The layered composition in BSDFs/OpenPbr/surface.part.glsl needs the BSDF without the
// outgoing cosine; evaluateOrenNayar below folds it in for callers that want f * cos(theta_o).
vec3 evaluateOrenNayarBsdf(
    const vec3 reflectance,
    const float roughness,
    const float cosThetaI,
    const float cosThetaO,
    const float cosBetween) {
    // Single scatter. The t term keeps 1 where s <= 0 rather than clamping the azimuthal factor to zero, which
    // is what lets the directional albedo above integrate in closed form.
    const float s = cosBetween - cosThetaI * cosThetaO;
    const float sOverT = s > 0.0f ? s / max(max(cosThetaI, cosThetaO), 1e-7f) : s;
    const vec3 singleScatter = reflectance * InvPI * orenNayarA(roughness) * (1.0f + roughness * sOverT);

    const float averageAlbedo = orenNayarAverageAlbedo(roughness);
    const vec3 multipleScatterAlbedo =
        reflectance * reflectance * averageAlbedo / max(vec3(1e-7f), 1.0f - reflectance * (1.0f - averageAlbedo));

    // The epsilons guard roughness 0, where all three deficits vanish together and the ratio is 0/0.
    const vec3 compensation = multipleScatterAlbedo * InvPI *
        max(1e-7f, 1.0f - orenNayarDirectionalAlbedo(cosThetaO, roughness)) *
        max(1e-7f, 1.0f - orenNayarDirectionalAlbedo(cosThetaI, roughness)) / max(1e-7f, 1.0f - averageAlbedo);

    return singleScatter + compensation;
}

vec3 evaluateOrenNayar(const vec3 reflectance, const float roughness, const vec3 wi, const vec3 wo) {
    if (wi.z <= 0.0f || wo.z <= 0.0f) {
        return vec3(0.0f);
    }

    return evaluateOrenNayarBsdf(reflectance, roughness, wi.z, wo.z, dot(wi, wo)) * wo.z;
}

#endif // CRISP_OREN_NAYAR_GLSL
