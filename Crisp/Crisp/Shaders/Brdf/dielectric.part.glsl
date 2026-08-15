#ifndef CRISP_DIELECTRIC_GLSL
#define CRISP_DIELECTRIC_GLSL

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

#endif // CRISP_DIELECTRIC_GLSL
