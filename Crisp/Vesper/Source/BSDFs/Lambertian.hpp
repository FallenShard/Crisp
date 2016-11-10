#pragma once

#include "BSDF.hpp"

namespace vesper
{
    class LambertianBSDF : public BSDF
    {
    public:
        LambertianBSDF(const VariantMap& params);
        ~LambertianBSDF();

        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
        virtual float pdf(const BSDF::Sample& bsdfSample) const override;
        virtual unsigned int getType() const override;

    private:
        Spectrum m_reflectance;
    };
}
