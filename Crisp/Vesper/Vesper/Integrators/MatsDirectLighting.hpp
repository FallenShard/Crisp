#pragma once

#include "Integrator.hpp"

namespace vesper
{
    struct Intersection;

    class MatsDirectLightingIntegrator : public Integrator
    {
    public:
        MatsDirectLightingIntegrator(const VariantMap& params = VariantMap());
        virtual ~MatsDirectLightingIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
    };
}