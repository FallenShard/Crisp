#include "Microfacet.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Math/Constants.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"
#include "MicrofacetDistributions/MicrofacetDistributionFactory.hpp"

namespace vesper
{
    MicrofacetBSDF::MicrofacetBSDF(const VariantMap& params)
    {
        m_lobe = Lobe::Diffuse;
        m_lobe |= Lobe::Glossy;

        std::string distribType = params.get<std::string>("distribution", "beckmann");
        m_distrib = MicrofacetDistributionFactory::create(distribType, params);

        m_intIOR = params.get<float>("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
        m_extIOR = params.get<float>("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));
        m_kd = params.get<Spectrum>("kd", Spectrum(0.5f));

        m_ks = 1.0f - m_kd.maxCoeff();
    }

    MicrofacetBSDF::~MicrofacetBSDF()
    {
    }

    Spectrum MicrofacetBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

        if (bsdfSample.measure != Measure::SolidAngle || cosThetaI <= 0.0f || cosThetaO <= 0.0f)
            return Spectrum(0.0f);

        glm::vec3 m = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        Spectrum diffuse = m_kd * InvPI;

        float F = Fresnel::dielectric(glm::dot(bsdfSample.wi, m), m_extIOR, m_intIOR);
        float D = m_distrib->D(m);
        float G = m_distrib->G(bsdfSample.wi, bsdfSample.wo, m);

        Spectrum specular = m_ks * F * D * G;

        return diffuse * cosThetaO + specular / (4.0f * cosThetaI);
    }

    Spectrum MicrofacetBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        if (CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f)
            return Spectrum(0.0f);

        bsdfSample.measure = Measure::SolidAngle;
        bsdfSample.eta     = 1.0f;

        auto sample = sampler.next2D();

        float cosThetaO = 0.0f;
        if (sample.x < m_ks)
        {
            glm::vec2 reusedSample(sample.x / m_ks, sample.y);

            auto m = m_distrib->sampleNormal(reusedSample);
            bsdfSample.wo = 2.0f * glm::dot(m, bsdfSample.wi) * m - bsdfSample.wi;
            bsdfSample.sampledLobe = Lobe::Glossy;
        }
        else
        {
            glm::vec2 reusedSample((sample.x - m_ks) / (1.0f - m_ks), sample.y);

            bsdfSample.wo = Warp::squareToCosineHemisphere(reusedSample);
            bsdfSample.sampledLobe = Lobe::Diffuse;
        }

        cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);
        if (cosThetaO < 0.0f)
            return Spectrum(0.0f);

        bsdfSample.pdf = pdf(bsdfSample);

        return eval(bsdfSample) / bsdfSample.pdf;
    }

    float MicrofacetBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        if (bsdfSample.measure != Measure::SolidAngle ||
            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
        {
            return 0.0f;
        }

        glm::vec3 m = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        float Jh = 1.0f / (4.0f * glm::dot(m, bsdfSample.wo));
        float specPdf = m_ks * m_distrib->pdf(m) * Jh;
        float diffPdf = (1.0f - m_ks) * Warp::squareToCosineHemispherePdf(bsdfSample.wo);

        return specPdf + diffPdf;
    }
}