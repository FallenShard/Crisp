#ifndef CRISP_FRESNEL_GLSL
#define CRISP_FRESNEL_GLSL

float fresnelDielectric(float cosThetaI, float extIor, float intIor, out float cosThetaT) {
    float etaI = extIor;
    float etaT = intIor;

    if (extIor == intIor) {
        cosThetaT = abs(cosThetaI);
        return 0.0f;
    }

    if (cosThetaI < 0.0f) {
        const float temp = etaI;
        etaI = etaT;
        etaT = temp;
        cosThetaI = -cosThetaI;
    }

    const float eta = etaI / etaT;
    const float sinThetaTSquared = eta * eta * (1.0f - cosThetaI * cosThetaI);
    if (sinThetaTSquared > 1.0f) {
        cosThetaT = 0.0f;
        return 1.0f;
    }

    cosThetaT = sqrt(1.0f - sinThetaTSquared);

    const float rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
    const float rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
    return (rs * rs + rp * rp) * 0.5f;
}

vec3 fresnelConductor(const float cosThetaI, const vec3 eta, const vec3 k) {
    const float cosThetaSquared = cosThetaI * cosThetaI;
    const float sinThetaSquared = 1.0f - cosThetaSquared;
    const vec3 etaSquared = eta * eta;
    const vec3 kSquared = k * k;

    const vec3 t0 = etaSquared - kSquared - sinThetaSquared;
    const vec3 a2b2 = sqrt(t0 * t0 + 4.0f * etaSquared * kSquared);
    const vec3 t1 = a2b2 + cosThetaSquared;
    const vec3 a = sqrt(0.5f * (a2b2 + t0));
    const vec3 t2 = 2.0f * a * cosThetaI;
    const vec3 rs = (t1 - t2) / (t1 + t2);

    const vec3 t3 = cosThetaSquared * a2b2 + sinThetaSquared * sinThetaSquared;
    const vec3 t4 = t2 * sinThetaSquared;
    const vec3 rp = rs * (t3 - t4) / (t3 + t4);

    return (rp + rs) * 0.5f;
}

#endif // CRISP_FRESNEL_GLSL
