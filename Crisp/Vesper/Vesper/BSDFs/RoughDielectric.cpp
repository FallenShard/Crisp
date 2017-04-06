#include "RoughDielectric.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Math/Constants.hpp"
#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"

namespace vesper
{
    namespace
    {
        inline glm::vec3 squareToBeckmann(const glm::vec2& sample, float alpha)
        {
            float theta = atan(sqrt(-alpha * alpha * log(1 - sample.x)));
            float phi = 2 * PI * sample.y;

            float sinTheta = sin(theta);

            return glm::vec3(sinTheta * cosf(phi), sinTheta * sinf(phi), cos(theta));
        }

        inline float mPdf(const glm::vec3& m)
        {

        }

        inline float chiPlus(float alpha)
        {
            return alpha > 0.0f ? 1.0f : 0.0f;
        }

        inline float D(const glm::vec3& m, const glm::vec3& n, float alpha)
        {
            float temp = CoordinateFrame::tanTheta(m) / alpha;
            float cosTheta = CoordinateFrame::cosTheta(m);
            float alphaCosTheta2 = cosTheta * cosTheta * alpha;

            return chiPlus(glm::dot(m, n)) * std::exp(-temp * temp) * InvPI / (alphaCosTheta2 * alphaCosTheta2);
        }
    }


    RoughDielectricBSDF::RoughDielectricBSDF(const VariantMap& params)
    {
        m_lobe = Lobe::Glossy;

        m_alpha = params.get<float>("alpha", 0.1f);
        m_intIOR = params.get<float>("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
        m_extIOR = params.get<float>("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));
    }

    RoughDielectricBSDF::~RoughDielectricBSDF()
    {
    }

    Spectrum RoughDielectricBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

        bool isReflection = cosThetaI * cosThetaI >= 0.0f;

        if (bsdfSample.measure != Measure::SolidAngle)
            return Spectrum(0.0f);

        float etaExt = m_extIOR, etaInt = m_intIOR;

        // If the angle was negative, we're coming from the inside, update relevant variables
        if (cosThetaI < 0.0f)
        {
            std::swap(etaExt, etaInt);
        }

        float eta = etaExt / etaInt;

        glm::vec3 m;
        if (isReflection)
        {
            float sgn = cosThetaI < 0.0f ? -1.0f : 1.0f;
            m = sgn * glm::normalize(bsdfSample.wi + bsdfSample.wo);
        }
        else
        {
            m = -glm::normalize(bsdfSample.wi * eta + bsdfSample.wo);
        }

        float cosThetaIm = glm::dot(bsdfSample.wi, m);
        float cosThetaOm = glm::dot(bsdfSample.wo, m);

        float cosThetaT = 0.0f;
        float fresnel = Fresnel::dielectric(cosThetaIm, etaExt, etaInt, cosThetaT);
        float G = smithBeckmannG1(bsdfSample.wi, m) * smithBeckmannG1(bsdfSample.wo, m);
        float D = evalBeckmann(m);

        if (isReflection)
        {
            return fresnel * G * D * 0.25f / std::abs(cosThetaI);
        }
        else
        {
            return std::abs(cosThetaIm * cosThetaOm) * (1 - fresnel) * G * D
                / (std::sqrt(eta * cosThetaIm + cosThetaOm) * std::abs(cosThetaI));
        }
    }

    Spectrum RoughDielectricBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        bsdfSample.measure     = Measure::SolidAngle;
        bsdfSample.sampledLobe = Lobe::Glossy;

        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        bool inside = false;

        // These may be swapped if we come "from the inside"
        float etaExt = m_extIOR, etaInt = m_intIOR;

        // If the angle was negative, we're coming from the inside, update relevant variables
        if (cosThetaI < 0.0f)
        {
            std::swap(etaExt, etaInt);
            inside = true;
        }

        auto m = squareToBeckmann(sampler.next2D(), m_alpha); // m
        auto mPdf = Warp::squareToBeckmannPdf(m, m_alpha); // D(m) * |m * n|

        auto cosThetaIm = glm::dot(bsdfSample.wi, m);
        float cosThetaT = 0.0f;
        float fresnel = Fresnel::dielectric(cosThetaIm, m_extIOR, m_intIOR, cosThetaT);
        float etaM = cosThetaIm < 0.0f ? m_intIOR / m_extIOR : m_extIOR / m_intIOR;

        bool isReflection = sampler.next1D() <= fresnel ? true : false;

        glm::vec3 wo;
        float woPdf;

        if (isReflection)
        {
            wo    = 2.0f * glm::dot(m, bsdfSample.wi) * m - bsdfSample.wi;
            woPdf = mPdf * 0.25f / std::abs(cosThetaIm);
            woPdf *= fresnel;
        }
        else
        {
            auto refrM = inside ? -m : m;
            auto refrCosThetaIm = inside ? -cosThetaIm : cosThetaIm;
            auto eta = etaExt / etaInt;

            wo    = eta * (refrCosThetaIm * refrM - bsdfSample.wi) - refrM * cosThetaT;
            float cosThetaOm = glm::dot(wo, m);
            woPdf = mPdf * std::abs(cosThetaOm) / std::sqrt(eta * cosThetaIm + cosThetaOm);
            woPdf *= 1 - fresnel;
        }

        //float cosThetaO = CoordinateFrame::cosTheta(wo);
        //if ((cosThetaO * cosThetaI > 0.0f) != isReflection)
        //    return Spectrum(0.0f);

        float G1wi = smithBeckmannG1(bsdfSample.wi, m);
        float G1wo = smithBeckmannG1(bsdfSample.wo, m);
        float D = evalBeckmann(m);

        Spectrum throughput = std::abs(cosThetaIm) * G1wi * G1wo / (std::abs(cosThetaI) * mPdf);

        return throughput;
    }

    float RoughDielectricBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
        auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

        bool isReflection = cosThetaI * cosThetaI >= 0.0f;

        if (bsdfSample.measure != Measure::SolidAngle)
            return 0.0f;

        float etaExt = m_extIOR, etaInt = m_intIOR;

        // If the angle was negative, we're coming from the inside, update relevant variables
        if (cosThetaI < 0.0f)
        {
            std::swap(etaExt, etaInt);
        }

        float eta = etaExt / etaInt;

        glm::vec3 m;
        if (isReflection)
        {
            float sgn = cosThetaI < 0.0f ? -1.0f : 1.0f;
            m = sgn * glm::normalize(bsdfSample.wi + bsdfSample.wo);
        }
        else
        {
            m = -glm::normalize(bsdfSample.wi * eta + bsdfSample.wo);
        }

        float cosThetaIm = glm::dot(bsdfSample.wi, m);
        float cosThetaOm = glm::dot(bsdfSample.wo, m);

        float cosThetaT = 0.0f;
        float fresnel = Fresnel::dielectric(cosThetaIm, etaExt, etaInt, cosThetaT);
        float mPdf = Warp::squareToBeckmannPdf(m, m_alpha);

        if (isReflection)
        {
            return fresnel * mPdf * 0.25f / std::abs(cosThetaIm);
        }
        else
        {
            return (1.0f - fresnel) * mPdf * std::abs(cosThetaOm) / std::sqrt(eta * cosThetaIm + cosThetaOm);
        }
    }

    float RoughDielectricBSDF::evalBeckmann(const glm::vec3& m) const
    {
        float temp = CoordinateFrame::tanTheta(m) / m_alpha;
        float cosTheta = CoordinateFrame::cosTheta(m);
        float cosTheta2 = cosTheta * cosTheta;

        return std::exp(-temp * temp) / (PI * m_alpha * m_alpha * cosTheta2 * cosTheta2);
    }

    float RoughDielectricBSDF::smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const
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