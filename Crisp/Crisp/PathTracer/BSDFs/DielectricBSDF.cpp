#include <Crisp/PathTracer/BSDFs/DielectricBSDF.hpp>

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/PathTracer/Optics/Fresnel.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp {
DielectricBSDF::DielectricBSDF(const VariantMap& params)
    : BSDF(Lobe::Delta) {
    m_intIOR = params.get("intIOR", Fresnel::getIOR(IndexOfRefraction::Glass));
    m_extIOR = params.get("extIOR", Fresnel::getIOR(IndexOfRefraction::Air));
}

Spectrum DielectricBSDF::eval(const BSDF::Sample& /*bsdfSample*/) const {
    return {0.0f};
}

Spectrum DielectricBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const {
    // Get the fresnel coefficient
    float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    float cosThetaT = 0.0f;
    float fresnel = Fresnel::dielectric(cosThetaI, m_extIOR, m_intIOR, cosThetaT);

    // Set the measure to discrete
    bsdfSample.measure = Measure::Discrete;
    bsdfSample.sampledLobe = Lobe::Delta;

    // If sample is less than fresnel, reflect, otherwise refract
    if (sampler.next1D() <= fresnel) {
        // Reflection in local coordinates
        bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);
        bsdfSample.eta = 1.0f;
        bsdfSample.pdf = fresnel;

        return {1.0f};
    }

    // Normal to use for refraction direction formula
    const glm::vec3 n(0.0f, 0.0f, 1.0f);

    // If the angle is negative, we're coming from the inside
    const float eta = cosThetaI < 0.0f ? m_intIOR / m_extIOR : m_extIOR / m_intIOR;

    // Set outgoing direction
    bsdfSample.wo = n * (eta * cosThetaI - sign(cosThetaI) * cosThetaT) - eta * bsdfSample.wi;
    bsdfSample.eta = eta;
    bsdfSample.pdf = 1.0f - fresnel;

    // Return the bsdf sample
    return {eta * eta};
}

float DielectricBSDF::pdf(const BSDF::Sample& /*bsdfSample*/) const {
    return 0.0f;
}
} // namespace crisp