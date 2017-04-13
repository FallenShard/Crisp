#include "RoughConductor.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"

#include "MicrofacetDistributions/MicrofacetDistributionFactory.hpp"

namespace vesper
{
    RoughConductorBSDF::RoughConductorBSDF(const VariantMap& params)
    {
        m_lobe = Lobe::Glossy;

        auto materialName = params.get("material", std::string("Au"));
        m_IOR = Fresnel::getComplexIOR(materialName);

        std::string distribType = params.get<std::string>("distribution", "beckmann");
        m_distrib = MicrofacetDistributionFactory::create(distribType, params);
    }

    RoughConductorBSDF::~RoughConductorBSDF()
    {
    }

    Spectrum RoughConductorBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

        if (bsdfSample.measure != Measure::SolidAngle || cosThetaI <= 0.0f || cosThetaO <= 0.0f)
            return Spectrum(0.0f);

        glm::vec3 m = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        Spectrum F = Fresnel::conductorFull(glm::dot(bsdfSample.wi, m), m_IOR);
        float D = m_distrib->D(m);
        float G = m_distrib->G(bsdfSample.wi, bsdfSample.wo, m);

        return F * D * G / (4.0f * cosThetaI);
    }

    Spectrum RoughConductorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        if (cosThetaI <= 0.0f)
            return Spectrum(0.0f);

        auto m = m_distrib->sampleNormal(sampler.next2D());

        float cosThetaIm = glm::dot(bsdfSample.wi, m);
        bsdfSample.wo = 2.0f * cosThetaIm * m - bsdfSample.wi;

        // Reflected vector must be on the same side as the original vector
        if (CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
            return Spectrum(0.0f);

        auto mPdf = m_distrib->pdf(m);

        bsdfSample.measure     = Measure::SolidAngle;
        bsdfSample.sampledLobe = Lobe::Glossy;
        bsdfSample.eta         = 1.0f;
        bsdfSample.pdf         = mPdf / (4.0f * cosThetaIm);
        
        Spectrum F = Fresnel::conductorFull(cosThetaIm, m_IOR);
        float G = m_distrib->G(bsdfSample.wi, bsdfSample.wo, m);
        float D = m_distrib->D(m);

        return F * G * cosThetaIm / (cosThetaI * CoordinateFrame::cosTheta(m));
    }

    float RoughConductorBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

        if (bsdfSample.measure != Measure::SolidAngle || cosThetaI < 0.0f || cosThetaO < 0.0f)
            return 0.0f;

        auto m = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        auto mPdf = m_distrib->pdf(m);

        float cosThetaIm = glm::dot(bsdfSample.wi, m);

        return mPdf / (4.0f * cosThetaIm);
    }
}