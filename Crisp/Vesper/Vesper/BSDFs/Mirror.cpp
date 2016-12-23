#include "Mirror.hpp"

#include "Math/CoordinateFrame.hpp"
#include "Samplers/Sampler.hpp"
#include "Core/Fresnel.hpp"

namespace vesper
{
    MirrorBSDF::MirrorBSDF(const VariantMap& params)
    {
    }

    MirrorBSDF::~MirrorBSDF()
    {
    }

    Spectrum MirrorBSDF::eval(const BSDF::Sample& bsdfSample) const
    {
        return Spectrum(0.0f);
    }

    Spectrum MirrorBSDF::sample(BSDF::Sample& bsdfSample, Sampler& sampler) const
    {
        // Set the measure to discrete
        bsdfSample.eta = 1.0f;
        bsdfSample.measure = Measure::Discrete;
        bsdfSample.sampledType = BSDF::Type::Delta;
        bsdfSample.wo = glm::vec3(-bsdfSample.wi.x, -bsdfSample.wi.y, bsdfSample.wi.z);

        return Spectrum(1.0f);
    }

    float MirrorBSDF::pdf(const BSDF::Sample& bsdfSample) const
    {
        return 0.0f;
    }

    unsigned int MirrorBSDF::getType() const
    {
        return BSDF::Type::Delta;
    }
}