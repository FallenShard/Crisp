#pragma once

#include "Integrator.hpp"

namespace vesper
{
    class NormalsIntegrator : public Integrator
    {
    public:
        NormalsIntegrator(const VariantMap& params = VariantMap());
        virtual ~NormalsIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;
    };
}