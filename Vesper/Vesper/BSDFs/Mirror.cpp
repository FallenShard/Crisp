#include "Mirror.hpp"

#include "Core/Fresnel.hpp"
#include "Samplers/Sampler.hpp"
#include <Crisp/Math/CoordinateFrame.hpp>

namespace crisp
{
MirrorBSDF::MirrorBSDF(const VariantMap& /*params*/)
    : BSDF(Lobe::Delta)
{
}

MirrorBSDF::~MirrorBSDF() {}

Spectrum MirrorBSDF::eval(const BSDF::Sample& /*bsdfSample*/) const
{
    return Spectrum(0.0f);
}

Spectrum MirrorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& /*sampler*/) const
{
    bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);
    bsdfSample.pdf = 1.0f;
    bsdfSample.measure = Measure::Discrete;
    bsdfSample.sampledLobe = Lobe::Delta;
    bsdfSample.eta = 1.0f;

    return Spectrum(1.0f);
}

float MirrorBSDF::pdf(const BSDF::Sample& /*bsdfSample*/) const
{
    return 0.0f;
}
} // namespace crisp