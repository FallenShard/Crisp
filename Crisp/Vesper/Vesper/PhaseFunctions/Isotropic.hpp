#pragma once

#include "PhaseFunction.hpp"

namespace vesper
{
    class IsotropicPhaseFunction : public PhaseFunction
    {
    public:
        IsotropicPhaseFunction(const VariantMap& params);
        ~IsotropicPhaseFunction();

        virtual float eval(const Sample& pfSample) const override;
        virtual float sample(Sample& pfSample, Sampler& sampler) const override;
        virtual float pdf(const Sample& pfSample) const override;
    };
}