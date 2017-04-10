#pragma once

#include "BSDF.hpp"
#include "Textures/Texture.hpp"
#include "MicrofacetDistributions/MicrofacetDistribution.hpp"

namespace vesper
{
    class RoughDielectricBSDF : public BSDF
    {
    public:
        RoughDielectricBSDF(const VariantMap& params);
        ~RoughDielectricBSDF();

        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
        virtual float pdf(const BSDF::Sample& bsdfSample) const override;

    private:
        float m_intIOR;
        float m_extIOR;
        std::unique_ptr<MicrofacetDistribution> m_distrib;
    };
}
