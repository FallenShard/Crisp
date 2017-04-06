#include "Dielectric.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"

namespace vesper
{
    DielectricBSDF::DielectricBSDF(const VariantMap& params)
    {
        m_lobe = LobeFlags(Lobe::Delta);
        m_intIOR = params.get("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
        m_extIOR = params.get("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));
    }

    DielectricBSDF::~DielectricBSDF()
    {
    }

    Spectrum DielectricBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        return Spectrum(0.0f);
    }

    Spectrum DielectricBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        // Get the fresnel coefficient
        float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        float cosThetaT = 0.0f;
        float fresnel = Fresnel::dielectric(cosThetaI, m_extIOR, m_intIOR, cosThetaT);

        // Set the measure to discrete
        bsdfSample.measure     = Measure::Discrete;
        bsdfSample.sampledLobe = Lobe::Delta;
        bsdfSample.pdf         = 1.0f;

        // If sample is less than fresnel, reflect, otherwise refract
        if (sampler.next1D() <= fresnel)
        {
            // Reflection in local coordinates
            bsdfSample.wo  = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);
            bsdfSample.eta = 1.0f;

            return Spectrum(1.0f);
        }

        // These may be swapped if we come "from the inside"
        float etaI = m_extIOR, etaT = m_intIOR;

        // Normal to use for refraction direction formula
        glm::vec3 n(0.0f, 0.0f, 1.0f);

        // If the angle was negative, we're coming from the inside, update relevant variables
        if (cosThetaI < 0.0f)
        {
            std::swap(etaI, etaT);
            cosThetaI = -cosThetaI;
            n = -n;
        }

        // Set eta ratio
        float etaRatio = etaI / etaT;
        
        // Set outgoing direction
        bsdfSample.wo  = -etaRatio * (bsdfSample.wi - cosThetaI * n) - n * cosThetaT;
        bsdfSample.eta = etaRatio;

        // Return the bsdf sample
        return Spectrum(etaRatio * etaRatio);
    }

    float DielectricBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        return 0.0f;
    }
}