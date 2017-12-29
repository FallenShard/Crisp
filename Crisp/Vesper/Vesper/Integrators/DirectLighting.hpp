#pragma once

#include "Integrator.hpp"

namespace vesper
{
    struct Intersection;

    class DirectLightingIntegrator : public Integrator
    {
    public:
        DirectLightingIntegrator(const VariantMap& params = VariantMap());
        virtual ~DirectLightingIntegrator();

        virtual void preprocess(Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray, IlluminationFlags flags = Illumination::Full) const override;
    };
}