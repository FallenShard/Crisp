#include "Isotropic.hpp"

#include "Math/Warp.hpp"
#include "Samplers/Sampler.hpp"

namespace vesper
{
    IsotropicPhaseFunction::IsotropicPhaseFunction(const VariantMap& params)
    {
    }
    
    IsotropicPhaseFunction::~IsotropicPhaseFunction()
    {
    }

    float IsotropicPhaseFunction::eval(const Sample& pfSample) const
    {
        return Warp::squareToUniformSpherePdf();
    }
    
    float IsotropicPhaseFunction::sample(Sample& pfSample, Sampler& sampler) const
    {
        pfSample.wo  = Warp::squareToUniformSphere(sampler.next2D());
        pfSample.pdf = Warp::squareToUniformSpherePdf();
        return 1.0f;
    }

    float IsotropicPhaseFunction::pdf(const Sample& pfSample) const
    {
        return Warp::squareToUniformSpherePdf();
    }
}