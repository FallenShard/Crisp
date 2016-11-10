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

        virtual void preprocess(const Scene* scene) override;
        virtual Spectrum Li(const Scene* scene, Sampler& sampler, Ray3& ray) const override;

    private:
        Spectrum lightImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
        Spectrum bsdfImportanceSample(const Scene* scene, Sampler& sampler, const Ray3& ray, const Intersection& its) const;
    };
}