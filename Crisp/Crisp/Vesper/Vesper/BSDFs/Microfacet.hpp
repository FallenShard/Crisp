#pragma once

#include "BSDF.hpp"
#include "Textures/Texture.hpp"
#include "MicrofacetDistributions/MicrofacetDistribution.hpp"

namespace crisp
{
    class MicrofacetBSDF : public BSDF
    {
    public:
        MicrofacetBSDF(const VariantMap& params);
        ~MicrofacetBSDF();

        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
        virtual float pdf(const BSDF::Sample& bsdfSample) const override;

    private:
        float m_intIOR;
        float m_extIOR;
        
        std::unique_ptr<MicrofacetDistribution> m_distrib;

        float m_ks;
        Spectrum m_kd;
    };
}
