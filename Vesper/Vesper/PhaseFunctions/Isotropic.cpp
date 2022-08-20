#include "Isotropic.hpp"

#include "Samplers/Sampler.hpp"
#include <Crisp/Math/Warp.hpp>

namespace crisp
{
IsotropicPhaseFunction::IsotropicPhaseFunction(const VariantMap& /*params*/) {}

IsotropicPhaseFunction::~IsotropicPhaseFunction() {}

float IsotropicPhaseFunction::eval(const Sample& /*pfSample*/) const
{
    return warp::squareToUniformSpherePdf();
}

float IsotropicPhaseFunction::sample(Sample& pfSample, Sampler& sampler) const
{
    pfSample.wo = warp::squareToUniformSphere(sampler.next2D());
    pfSample.pdf = warp::squareToUniformSpherePdf();
    return 1.0f;
}

float IsotropicPhaseFunction::pdf(const Sample& /*pfSample*/) const
{
    return warp::squareToUniformSpherePdf();
}
} // namespace crisp