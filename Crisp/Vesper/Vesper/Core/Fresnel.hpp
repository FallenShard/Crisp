#pragma once

#include <algorithm>

namespace vesper
{
    enum class IndexOfRefraction
    {
        Vacuum,
        Air,
        Ice,
        Water,
        Glass,
        Sapphire,
        Diamond
    };

    class Fresnel
    {
    public:
        inline static float dielectric(float cosThetaI, float extIOR, float intIOR)
        {
            float etaI = extIOR, etaT = intIOR;

            // If indices of refraction are the same, no fresnel effects
            if (extIOR == intIOR)
                return 0.0f;

            // if cosThetaI is < 0, it means the ray is coming from inside the object
            if (cosThetaI < 0.0f)
            {
                std::swap(etaI, etaT);
                cosThetaI = -cosThetaI;
            }

            float eta = etaI / etaT;
            float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

            // Total internal reflection
            if (sinThetaTSqr > 1.0f)
                return 1.0f;

            float cosThetaT = std::sqrtf(1.0f - sinThetaTSqr);

            float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
            float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
            return (Rs * Rs + Rp * Rp) / 2.0f;
        }

        inline static float getIOR(IndexOfRefraction iorMaterial)
        {
            switch (iorMaterial)
            {
            case IndexOfRefraction::Vacuum:   return 1.0f;
            case IndexOfRefraction::Air:      return 1.00029f;
            case IndexOfRefraction::Ice:      return 1.31f;
            case IndexOfRefraction::Water:    return 1.33f;
            case IndexOfRefraction::Glass:    return 1.52f;
            case IndexOfRefraction::Sapphire: return 1.77f;
            case IndexOfRefraction::Diamond:  return 2.417f;
            default:                          return 1.0f;
            }
        };
    };
}