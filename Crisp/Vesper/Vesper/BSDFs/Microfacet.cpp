#include "Microfacet.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Math/Constants.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"

namespace vesper
{
    MicrofacetBSDF::MicrofacetBSDF(const VariantMap& params)
    {
        m_lobe = Lobe::Diffuse;
        m_lobe |= Lobe::Glossy;

        m_alpha = params.get<float>("alpha", 0.1f);
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

        glm::vec3 halfVec = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        Spectrum diffuse = m_kd * InvPI;

        Spectrum specular = m_ks * evalBeckmann(halfVec) * Fresnel::dielectric(glm::dot(halfVec, bsdfSample.wi), m_extIOR, m_intIOR)
            * smithBeckmannG1(bsdfSample.wi, halfVec) * smithBeckmannG1(bsdfSample.wo, halfVec);
        float specDenom = 4 * cosThetaI * cosThetaO;

        return diffuse + specular / specDenom;
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

            auto m = Warp::squareToBeckmann(reusedSample, m_alpha);
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

        return eval(bsdfSample) / bsdfSample.pdf * cosThetaO;
    }

    float MicrofacetBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        if (bsdfSample.measure != Measure::SolidAngle ||
            CoordinateFrame::cosTheta(bsdfSample.wi) <= 0.0f ||
            CoordinateFrame::cosTheta(bsdfSample.wo) <= 0.0f)
        {
            return 0.0f;
        }

        glm::vec3 halfVec = glm::normalize(bsdfSample.wi + bsdfSample.wo);

        float Jh = 1.0f / (4.0f * glm::dot(halfVec, bsdfSample.wo));
        float specPdf = m_ks * Warp::squareToBeckmannPdf(halfVec, m_alpha) * Jh;
        float diffPdf = (1.0f - m_ks) * Warp::squareToCosineHemispherePdf(bsdfSample.wo);

        return specPdf + diffPdf;
    }

    float MicrofacetBSDF::evalBeckmann(const glm::vec3& m) const
    {
        float temp = CoordinateFrame::tanTheta(m) / m_alpha;
        float cosTheta = CoordinateFrame::cosTheta(m);
        float cosTheta2 = cosTheta * cosTheta;

        return std::exp(-temp * temp) / (PI * m_alpha * m_alpha * cosTheta2 * cosTheta2);
    }

    float MicrofacetBSDF::smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const
    {
        float tanTheta = CoordinateFrame::tanTheta(v);
        if (tanTheta == 0.0f)
            return 1.0f;

        // Back-surface check
        if (glm::dot(v, m) * CoordinateFrame::cosTheta(v) <= 0)
            return 0.0f;

        float b = 1.0f / (m_alpha * tanTheta);
        if (b >= 1.6f)
            return 1.0f;

        float b2 = b * b;

        return (3.535f * b + 2.181f * b2) / (1.0f + 2.276f * b + 2.577f * b2);
    }
}