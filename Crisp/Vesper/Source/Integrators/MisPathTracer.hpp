#pragma once

#include "Integrator.hpp"

namespace vesper
{
    class MisPathTracerIntegrator : public Integrator
    {
    public:
        MisPathTracerIntegrator(const VariantMap& attributes);
        virtual ~MisPathTracerIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;
    };
}