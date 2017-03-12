#pragma once

#include "BSDF.hpp"
#include "Textures/Texture.hpp"

namespace vesper
{
    class MicrofacetBSDF : public BSDF
    {
    public:
        MicrofacetBSDF(const VariantMap& params);
        ~MicrofacetBSDF();

        virtual Spectrum eval(const BSDF::Sample& bsdfSample) const override;
        virtual Spectrum sample(BSDF::Sample& bsdfSample, Sampler& sampler) const override;
        virtual float pdf(const BSDF::Sample& bsdfSample) const override;
        virtual unsigned int getType() const override;

    private:
        float evalBeckmann(const glm::vec3& m) const;
        float smithBeckmannG1(const glm::vec3& v, const glm::vec3& m) const;


        float m_intIOR;
        float m_extIOR;
        float m_alpha;
        float m_ks;
        Spectrum m_kd;
    };
}
