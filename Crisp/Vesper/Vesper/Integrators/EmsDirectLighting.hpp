#pragma once

#include "Integrator.hpp"

namespace vesper
{
    struct Intersection;

    class EmsDirectLightingIntegrator : public Integrator
    {
    public:
        EmsDirectLightingIntegrator(const VariantMap& params = VariantMap());
        virtual ~EmsDirectLightingIntegrator();

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;
    };
}