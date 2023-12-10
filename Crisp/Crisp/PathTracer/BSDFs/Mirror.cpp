#include <Crisp/PathTracer/BSDFs/Mirror.hpp>

#include <Crisp/Math/CoordinateFrame.hpp>
#include <Crisp/Optics/Fresnel.hpp>
#include <Crisp/PathTracer/Samplers/Sampler.hpp>

namespace crisp {
MirrorBSDF::MirrorBSDF(const VariantMap& /*params*/)
    : BSDF(Lobe::Delta) {}

Spectrum MirrorBSDF::eval(const BSDF::Sample& /*bsdfSample*/) const {
    return Spectrum::zero();
}

Spectrum MirrorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& /*sampler*/) const {
    bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);
    bsdfSample.pdf = 1.0f;
    bsdfSample.measure = Measure::Discrete;
    bsdfSample.sampledLobe = Lobe::Delta;
    bsdfSample.eta = 1.0f;

    return {1.0f};
}

float MirrorBSDF::pdf(const BSDF::Sample& /*bsdfSample*/) const {
    return 0.0f;
}
} // namespace crisp