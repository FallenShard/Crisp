#include <Crisp/PathTracer/BSDFs/RoughDielectric.hpp>

#include <Crisp/Math/Constants.hpp>
#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Math/Warp.hpp>
#include <Crisp/PathTracer/Optics/Fresnel.hpp>

#include <Crisp/PathTracer/BSDFs/MicrofacetDistributions/MicrofacetDistributionFactory.hpp>

namespace crisp {
namespace {
inline float jacobianReflect(float cosThetaO) {
    return 1.0f / (4.0f * std::abs(cosThetaO));
}

inline float jacobianRefract(float eta, float cosThetaIm, float cosThetaOm) {
    float a = eta * cosThetaIm + cosThetaOm;
    return std::abs(cosThetaOm / (a * a));
}

inline glm::vec3 hR(const glm::vec3& wi, const glm::vec3& wo) {
    return glm::normalize(wi + wo);
}

inline glm::vec3 hT(const glm::vec3& wi, const glm::vec3& wo, float eta) {
    return glm::normalize(eta * wi + wo);
}

inline glm::vec3 reflect(const glm::vec3& wi, const glm::vec3& n, float cosThetaI) {
    return 2.0f * cosThetaI * n - wi;
}

inline glm::vec3 refract(const glm::vec3& wi, const glm::vec3& n, float eta, float cosThetaI, float cosThetaT) {
    return n * (eta * cosThetaI - sign(cosThetaI) * cosThetaT) - eta * wi;
}
} // namespace

RoughDielectricBSDF::RoughDielectricBSDF(const VariantMap& params)
    : BSDF(Lobe::Glossy) {
    m_intIOR = params.get<float>("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
    m_extIOR = params.get<float>("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));

    std::string distribType = params.get<std::string>("distribution", "beckmann");

    m_distrib = MicrofacetDistributionFactory::create(distribType, params);
}

RoughDielectricBSDF::~RoughDielectricBSDF() {}

Spectrum RoughDielectricBSDF::eval(const BSDF::Sample& bsdfSample) const {
    if (bsdfSample.measure != Measure::SolidAngle) {
        return Spectrum(0.0f);
    }

    auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

    bool isReflection = cosThetaI * cosThetaO > 0.0f;

    // If the angle is negative, we're coming from the inside
    float eta = cosThetaI < 0.0f ? m_intIOR / m_extIOR : m_extIOR / m_intIOR;

    glm::vec3 m;
    if (isReflection) {
        m = sign(cosThetaI) * hR(bsdfSample.wi, bsdfSample.wo);
    } else {
        m = -hT(bsdfSample.wi, bsdfSample.wo, eta);
    }

    float cosThetaIm = glm::dot(bsdfSample.wi, m);
    float cosThetaOm = glm::dot(bsdfSample.wo, m);

    m *= sign(CoordinateFrame::cosTheta(m));

    float F = Fresnel::dielectric(cosThetaIm, m_extIOR, m_intIOR);
    float D = m_distrib->D(m);
    float G = m_distrib->G(bsdfSample.wi, bsdfSample.wo, m);

    if (isReflection) {
        return F * D * G * jacobianReflect(cosThetaI);
    } else {
        float val = (1.0f - F) * D * G * jacobianRefract(eta, cosThetaIm, cosThetaOm);
        return val * std::abs(cosThetaIm / cosThetaI) * eta * eta;
    }
}

Spectrum RoughDielectricBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const {
    bsdfSample.measure = Measure::SolidAngle;
    bsdfSample.sampledLobe = Lobe::Glossy;

    auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);

    auto m = m_distrib->sampleNormal(sampler.next2D());
    auto mPdf = m_distrib->pdf(m);

    float cosThetaIm = glm::dot(bsdfSample.wi, m);
    float cosThetaTm = 0.0f;
    float fresnel = Fresnel::dielectric(cosThetaIm, m_extIOR, m_intIOR, cosThetaTm);
    float etaM = cosThetaIm < 0.0f ? m_intIOR / m_extIOR : m_extIOR / m_intIOR;

    float weight = 1.0f;
    if (sampler.next1D() <= fresnel) {
        bsdfSample.wo = reflect(bsdfSample.wi, m, cosThetaIm);
        bsdfSample.pdf = fresnel * mPdf * jacobianReflect(cosThetaIm);
        bsdfSample.eta = 1.0f;

        // Check if we reflected "into" the surface
        if (CoordinateFrame::cosTheta(bsdfSample.wo) * cosThetaI < 0.0f) {
            return Spectrum(0.0f);
        }
    } else {
        bsdfSample.wo = refract(bsdfSample.wi, m, etaM, cosThetaIm, cosThetaTm);
        bsdfSample.pdf = (1.0f - fresnel) * mPdf * jacobianRefract(etaM, cosThetaIm, glm::dot(bsdfSample.wo, m));
        bsdfSample.eta = etaM;

        weight = etaM * etaM;

        if (CoordinateFrame::cosTheta(bsdfSample.wo) * cosThetaI >= 0.0f) {
            return Spectrum(0.0f);
        }
    }

    float G = m_distrib->G(bsdfSample.wi, bsdfSample.wo, m);

    return std::abs(cosThetaIm / (cosThetaI * CoordinateFrame::cosTheta(m))) * weight * G;
}

float RoughDielectricBSDF::pdf(const BSDF::Sample& bsdfSample) const {
    if (bsdfSample.measure != Measure::SolidAngle) {
        return 0.0f;
    }

    auto cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    auto cosThetaO = CoordinateFrame::cosTheta(bsdfSample.wo);

    bool isReflection = cosThetaI * cosThetaO > 0.0f;

    // If the angle is positive, we're coming from the "proper" side
    float eta = cosThetaI < 0.0f ? m_intIOR / m_extIOR : m_extIOR / m_intIOR;

    glm::vec3 m;
    if (isReflection) {
        m = sign(cosThetaI) * hR(bsdfSample.wi, bsdfSample.wo);
    } else {
        m = -hT(bsdfSample.wi, bsdfSample.wo, eta);
    }

    float cosThetaIm = glm::dot(bsdfSample.wi, m);
    float cosThetaOm = glm::dot(bsdfSample.wo, m);
    float F = Fresnel::dielectric(cosThetaIm, m_extIOR, m_intIOR);

    m *= sign(CoordinateFrame::cosTheta(m));
    float mPdf = m_distrib->pdf(m);

    if (isReflection) {
        return std::abs(F * mPdf * jacobianReflect(cosThetaIm));
    } else {
        return std::abs((1.0f - F) * mPdf * jacobianRefract(eta, cosThetaIm, cosThetaOm));
    }
}
} // namespace crisp