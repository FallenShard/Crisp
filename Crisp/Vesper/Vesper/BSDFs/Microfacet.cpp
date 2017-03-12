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
        m_alpha = params.get<float>("alpha", 0.1f);
        m_intIOR = params.get<float>("intIOR", 1.5046f);
        m_extIOR = params.get<float>("extIOR", 1.000277f);
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
        bsdfSample.eta = 1.0f;
        bsdfSample.sampledType = Glossy;

        auto sample = sampler.next2D();

        if (sample.x < m_ks)
        {
            glm::vec2 reusedSample(sample.x / m_ks, sample.y);

            auto n = Warp::squareToBeckmann(reusedSample, m_alpha);

            bsdfSample.wo = 2.0f * glm::dot(n, bsdfSample.wi) * n - bsdfSample.wi;
            
            float cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

            if (cosThetaO < 0.0f)
                return Spectrum(0.0f);

            return eval(bsdfSample) / pdf(bsdfSample) * cosThetaO;
        }
        else
        {
            glm::vec2 reusedSample((sample.x - m_ks) / (1.0f - m_ks), sample.y);

            bsdfSample.wo = Warp::squareToCosineHemisphere(reusedSample);

            return eval(bsdfSample) / pdf(bsdfSample) * CoordinateFrame::cosTheta(bsdfSample.wo);
        }
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

    unsigned int MicrofacetBSDF::getType() const
    {
        return Glossy;
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

        if (glm::dot(v, m) * CoordinateFrame::cosTheta(v) <= 0)
            return 0.0f;

        float a = 1.0f / (m_alpha * tanTheta);
        if (a >= 1.6f)
            return 1.0f;

        float a2 = a * a;

        return (3.535f * a + 2.181f * a2) / (1.0f + 2.276f * a + 2.577f * a2);
    }
}