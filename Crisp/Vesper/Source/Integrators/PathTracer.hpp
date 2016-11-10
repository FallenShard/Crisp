#pragma once

#include "Integrator.hpp"

namespace vesper
{
    class PathTracerIntegrator : public Integrator
    {
    public:
        PathTracerIntegrator(const VariantMap& attributes);
        virtual ~PathTracerIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;
    };
}