#pragma once

#include "Integrator.hpp"

namespace vesper
{
    class AmbientOcclusionIntegrator : public Integrator
    {
    public:
        AmbientOcclusionIntegrator(const VariantMap& params = VariantMap());
        virtual ~AmbientOcclusionIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;

    private:
        float m_rayLength;
    };
}