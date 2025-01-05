#include <Crisp/PathTracer/BSDFs/SmoothConductor.hpp>

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/PathTracer/Optics/Fresnel.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp {
SmoothConductorBSDF::SmoothConductorBSDF(const VariantMap& params)
    : BSDF(Lobe::Delta) {
    auto materialName = params.get("material", std::string("Au"));
    m_IOR = Fresnel::getComplexIOR(materialName);
}

SmoothConductorBSDF::~SmoothConductorBSDF() {}

Spectrum SmoothConductorBSDF::eval(const BSDF::Sample& /*bsdfSample*/) const {
    return Spectrum(0.0f);
}

Spectrum SmoothConductorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& /*sampler*/) const {
    float cosThetaI = CoordinateFrame::cosTheta(bsdfSample.wi);
    if (cosThetaI < 0.0f) {
        return Spectrum(0.0f);
    }

    bsdfSample.measure = Measure::Discrete;
    bsdfSample.sampledLobe = Lobe::Delta;
    bsdfSample.pdf = 1.0f;
    bsdfSample.eta = 1.0f;
    bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);

    return Fresnel::conductorFull(cosThetaI, m_IOR);
}

float SmoothConductorBSDF::pdf(const BSDF::Sample& /*bsdfSample*/) const {
    return 0.0f;
}
} // namespace crisp